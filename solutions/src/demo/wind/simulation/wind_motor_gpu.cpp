/**
 * @file wind_motor_gpu.cpp
 * @brief 風発生器の GPU 寄与と入力キャッシュを管理する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "wind_motor_gpu.h"
#include "wind_common.h"
#include "motor_field.h"
#include "shader_sources.hpp"
#include "../defs/wind_region_constants.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace demo::wind::simulation::detail
{
namespace windDefs = demo::wind::defs;
namespace
{
/** 
 *  @brief シェーダーストレージに渡す std430 形式の発生器設定。
 *  @details 位置は管理空間の原点からの相対座標で、各項目の配置は発生器シェーダーと一致させる。
 */
struct alignas(32) GpuMotor
{
    std::array<double, 4> positionRadius;       // 発生器の位置と半径
    std::array<double, 4> directionLength;      // 発生器の方向と長さ
    std::array<double, 4> parameters;           // X:風速、Y:形状（球0・円錐1・円柱2）、Z:発生方法（継続0・瞬間1）、W:距離減衰フラグ（有効1）
};

static_assert(sizeof(GpuMotor) == 96);                              // GpuMotor のサイズは 96 バイトであることを確認
static_assert(offsetof(GpuMotor, directionLength) == 32);           // directionLength のオフセットは 32 バイトであることを確認
static_assert(offsetof(GpuMotor, parameters) == 64);                // parameters のオフセットは 64 バイトであることを確認
static_assert(static_cast<int>(motor::Shape::Sphere) == 0 &&        // Shape::Sphere の値は 0 であることを確認
              static_cast<int>(motor::Shape::Cone) == 1 &&          // Shape::Cone の値は 1 であることを確認
              static_cast<int>(motor::Shape::Cylinder) == 2);       // Shape::Cylinder の値は 2 であることを確認
static_assert(static_cast<int>(motor::Mode::Continuous) == 0 &&     // Mode::Continuous の値は 0 であることを確認
              static_cast<int>(motor::Mode::Instant) == 1);         // Mode::Instant の値は 1 であることを確認

/**
 * @brief 要素数がシェーダーの添字と GPU バッファーのサイズに収まるか判定する。
 * @param i_count 格納する要素数。
 * @param i_elementBytes 1 要素のサイズ（バイト）。
 * @param i_maximum GPU が許容するバッファーの最大サイズ（バイト）。
 * @return 全ての上限に収まる場合は true。
 * @pre i_elementBytes は 0 より大きい。
 */
constexpr bool FitsBufferLimits(const std::size_t i_count, const std::size_t i_elementBytes,
                                const std::size_t i_maximum)
{
    // 掛け算前に上限を要素数に換算し、サイズ計算時のあふれを防ぐ。
    const auto indexLimit = static_cast<std::size_t>(std::numeric_limits<int>::max());
    const auto addressLimit = static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
    return i_count <= indexLimit &&
           i_count <= i_maximum / i_elementBytes &&
           i_count <= addressLimit / i_elementBytes;
}

/**
 * @brief セル別寄与と発生器設定のバッファーが各上限に収まるか検証する。
 * @param i_count セル数。
 * @param i_motorCount 発生器数。
 * @param i_maximum GPU が許容するバッファーの最大サイズ（バイト）。
 * @throws std::runtime_error いずれかの上限を超える場合。
 */
void ValidateBufferLimits(const std::size_t i_count, const std::size_t i_motorCount, const GLint64 i_maximum)
{
    if (i_maximum < 0 ||
        !FitsBufferLimits(i_count, sizeof(MotorCell), static_cast<std::size_t>(i_maximum)) ||
        !FitsBufferLimits(i_motorCount, sizeof(GpuMotor), static_cast<std::size_t>(i_maximum)))
    {
        throw std::runtime_error("発生器の情報が GPU バッファーの上限を超えています。");
    }
}

static_assert(FitsBufferLimits(0, 4, 8));  // 要素がない場合も受け入れる。
static_assert(FitsBufferLimits(2, 4, 8));  // GPU バッファーの容量と一致する場合は受け入れる。
static_assert(!FitsBufferLimits(3, 4, 8)); // GPU バッファーの容量を超える場合は拒否する。

// シェーダーの整数添字の上限は受け入れ、上限を 1 超える要素数は拒否する。
static_assert(FitsBufferLimits(static_cast<std::size_t>(std::numeric_limits<int>::max()), 1,
                               static_cast<std::size_t>(std::numeric_limits<int>::max())));
static_assert(!FitsBufferLimits(static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1, 1,
                                std::numeric_limits<std::size_t>::max()));

// GPU の容量内でも、OpenGL のバッファーサイズ型で合計バイト数を表現できなければ拒否する。
static_assert(!FitsBufferLimits(2, static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / 2 + 1,
                                std::numeric_limits<std::size_t>::max()));

}

/**
 * @brief 発生器の形状と風の寄与を計算するコンピュートプログラムを作成する。
 * @throws std::runtime_error シェーダーの作成、コンパイルまたはリンクに失敗した場合。
 * @pre 有効な OpenGL 4.3 コンテキスト上で呼び出す。
 */
WindMotorGpu::WindMotorGpu() : m_uProgram(Compile(shaders::windMotor)) {}

/**
 * @brief 所有する GPU バッファーとコンピュートプログラムを解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
WindMotorGpu::~WindMotorGpu()
{
    if (m_uMotorBuffer != 0) { glDeleteBuffers(1, &m_uMotorBuffer); }
    if (m_uSettingsBuffer != 0) { glDeleteBuffers(1, &m_uSettingsBuffer); }
    if (m_uProgram != 0) { glDeleteProgram(m_uProgram); }
}

/**
 * @brief 発生器と管理空間の入力をキャッシュと比較し、今回の更新入力を用意する。
 * @param i_motors 更新時に使用する発生器のスナップショット。
 * @param i_origin 管理空間の最小ワールド座標（整数メートル）。
 * @param i_size 格子の XYZ 各軸のセル数。
 * @param i_subdivisions 占有率を近似する際の各軸の分割数。
 * @return 入力と、GPU 上の寄与を再生成する必要があるかを示す情報。
 * @details 入力が変わった場合は旧キャッシュを破棄する。新しい入力の確定は Complete() が行う。
 * @pre 引数は呼び出し側で検証済みとし、発生器の方向は正規化可能とする。
 */
WindMotorInput WindMotorGpu::Prepare(std::vector<motor::Motor>                      i_motors,
                                    const std::array<int, windDefs::kAxisCount>&    i_origin,
                                    const std::array<int, windDefs::kAxisCount>&    i_size,
                                    const int                                       i_subdivisions)
{
    // 内容を比較してキャッシュが有効かどうかを判定する
    auto key            =   MotorCacheKey{std::move(i_motors), i_origin, i_size, i_subdivisions};
    const auto rebuilt  =   !m_instCache || *m_instCache != key;
    if (rebuilt) { m_instCache.reset(); }
    return {std::move(key), rebuilt};
}

/**
 * @brief 必要に応じて発生器のセル別寄与を GPU で再計算し、風更新用に結合する。
 * @param i_input Prepare() で得た今回の発生器入力。
 * @param i_query GPU 時間クエリの ID。計測しない場合は 0。
 * @param o_recorded 再計算を実行した場合に true を設定する記録状態。使用しない場合は null。
 * @param o_pending 再計算を実行した場合に true を設定する結果の取得待ち状態。使用しない場合は null。
 * @throws std::runtime_error GPU の容量上限を超えた場合、または GPU 処理に失敗した場合。
 * @details 入力が変わらなければ前回の寄与を再利用する。
 * シェーダーストレージの結合点 0 を変更し、再計算時は結合点 1 とプログラムも変更する。
 * @pre 有効な OpenGL 4.3 コンテキスト上で呼び出す。
 * @pre o_recorded が null でない場合、o_pending も有効なポインターとする。
 */
void WindMotorGpu::Apply(const WindMotorInput&  i_input,
                         const GLuint           i_query,
                         bool*                  o_recorded,
                         bool*                  o_pending)
{
    const auto& size = i_input.key.size;
    static_assert(sizeof(MotorCell) == 8 * sizeof(float));

    // 以前の OpenGL エラーを確認し、セル別寄与と発生器設定を格納できる容量を調べる。
    CheckGl("発生器の風の準備");
    auto maximum = GLint64{};
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maximum);
    const auto count        =   static_cast<std::size_t>(size[windDefs::kAxisXIndex]) *
                                static_cast<std::size_t>(size[windDefs::kAxisYIndex]) *
                                static_cast<std::size_t>(size[windDefs::kAxisZIndex]);
    const auto motorCount   =   i_input.key.motors.size();

    // バッファーの作成前に、両方のデータが添字と容量の上限に収まるか検証する。
    ValidateBufferLimits(count, motorCount, maximum);

    // セル別寄与と発生器設定のバッファーを必要に応じて作成する。
    if (m_uMotorBuffer == 0) { glGenBuffers(1, &m_uMotorBuffer); }
    if (m_uSettingsBuffer == 0) { glGenBuffers(1, &m_uSettingsBuffer); }
    if (m_uMotorBuffer == 0 || m_uSettingsBuffer == 0)
    {
        throw std::runtime_error("発生器のバッファーを作成できません。");
    }

    // 入力が変わった場合だけ寄与を再計算し、それ以外は前回の結果を再利用する。
    if (i_input.rebuilt)
    {
        auto motors = std::vector<GpuMotor>{};
        motors.reserve(motorCount);

        // 位置を管理空間の原点からの相対値にし、方向を正規化してシェーダー用の形式に詰める。
        for (const auto& motor : i_input.key.motors)
        {
            const auto& settings = motor.settings;
            const auto length = std::hypot(static_cast<double>(settings.direction[windDefs::kAxisXIndex]),
                                           static_cast<double>(settings.direction[windDefs::kAxisYIndex]),
                                           static_cast<double>(settings.direction[windDefs::kAxisZIndex]));
            auto gpuMotor = GpuMotor{};
            for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
            {
                gpuMotor.positionRadius[axis] = static_cast<double>(settings.position[axis]) -
                                                i_input.key.origin[axis];
                gpuMotor.directionLength[axis] = settings.direction[axis] / length;
            }

            // 各 4 成分配列の末尾には、シェーダーが参照する半径と長さを格納する。
            gpuMotor.positionRadius[windDefs::kAxisCount] = static_cast<double>(settings.diameter) * 0.5;
            gpuMotor.directionLength[windDefs::kAxisCount] = static_cast<double>(settings.length);
            gpuMotor.parameters = {static_cast<double>(settings.speed),
                                   static_cast<double>(settings.shape),
                                   static_cast<double>(settings.mode),
                                   settings.distanceFalloff ? 1.0 : 0.0};
            motors.push_back(gpuMotor);
        }

        // セル別寄与の出力先を確保し、発生器設定を GPU に転送する。発生器がなくても設定用に 1 件分を確保する。
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_uMotorBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(count * sizeof(MotorCell)),
                     nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_uSettingsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     static_cast<GLsizeiptr>(std::max(motorCount, std::size_t{1}) * sizeof(GpuMotor)),
                     motors.empty() ? nullptr : motors.data(), GL_STREAM_DRAW);

        // シェーダーに出力先、発生器設定と格子・占有率の条件を渡す。
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_uMotorBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_uSettingsBuffer);
        glUseProgram(m_uProgram);
        glUniform3iv(Uniform(m_uProgram, "size"), 1,
                     size.data());
        glUniform1i(Uniform(m_uProgram, "motorCount"),
                    static_cast<GLint>(motorCount));
        glUniform1i(Uniform(m_uProgram, "subdivisions"),
                    i_input.key.subdivisions);

        // 4×4×4 セル単位で寄与を計算し、クエリがある場合は GPU の実行時間を記録する。
        auto query = ElapsedQuery(i_query);
        glDispatchCompute(static_cast<GLuint>((size[windDefs::kAxisXIndex] + 3) / 4),
                          static_cast<GLuint>((size[windDefs::kAxisYIndex] + 3) / 4),
                          static_cast<GLuint>((size[windDefs::kAxisZIndex] + 3) / 4));
        query.End();
        if (o_recorded != nullptr)
        {
            *o_recorded = true;
            *o_pending = true;
        }

        // 計算した寄与を後続の風更新から参照できるようにし、GPU エラーを通知する。
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        CheckGl("発生器の風の計算");
    }

    // 再計算しなかった場合も、外部で変更された結合を戻して風更新に寄与を渡す。
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_uMotorBuffer);
    CheckGl("発生器の風の結合");
}

/**
 * @brief 風の更新成功後に瞬間発生器を解放し、再計算した入力をキャッシュに確定する。
 * @param io_input 今回の更新に使用した入力。再計算した場合はキャッシュへ移動する。
 * @param io_motors 瞬間発生器を解放する管理器。
 * @pre 風の更新が成功し、瞬間発生器がある場合は GPU の更新完了を確認済みとする。
 * @pre 更新中に同じ発生器管理器を他のスレッドから変更しない。
 */
void WindMotorGpu::Complete(WindMotorInput&& io_input, motor::WindMotors& io_motors)
{
    for (const auto& motor : io_input.key.motors)
    {
        if (motor.settings.mode == motor::Mode::Instant) { io_motors.CompleteInstant(motor.id); }
    }
    if (io_input.rebuilt) { m_instCache.emplace(std::move(io_input.key)); }
}
}
