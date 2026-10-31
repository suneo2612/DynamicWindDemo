/**
 * @file wind_grid.cpp
 * @brief GPU のコンピュート処理で風を隣接セルに分配する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "wind_grid.h"
#include "wind_common.h"
#include "wind_motor_gpu.h"
#include "shader_sources.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace demo::wind::simulation
{
namespace
{
using detail::CheckGl;
using detail::Compile;
using detail::ElapsedQuery;
using detail::kScale;
using detail::Uniform;
using detail::ValidateOrigin;
using detail::ValidateWind;

/**
 *  @brief タイマーのスロット数。
 */
constexpr auto kTimerSlotCount  =   std::size_t{4};

/**
 * @brief WindGrid が使うコンピュートプログラムの配列位置。
 */
enum class ComputeProgram : std::size_t
{
    WindClear,   // 格子の初期化と移動
    WindScatter, // 隣接セルへの風の分配
    WindFinish,  // 境界風と減衰の反映
    Count        // プログラム数。配列サイズの決定に使用
};

/**
 * @brief コンピュートプログラムの列挙値を配列添字へ変換する。
 * @param i_program 添字へ変換するプログラム種別。
 * @return 対応するプログラム配列の添字。
 */
constexpr auto ProgramIndex(const ComputeProgram i_program) noexcept
{
    return static_cast<std::size_t>(i_program);
}

/** 
 *  @brief 呼び出し側の転送設定に影響されず CPU の初期値を転送し、元の設定に戻す。
 */
struct UploadState
{
    GLint texture{};                    // 現在の 3D テクスチャのバインディング
    GLint buffer{};                     // 現在のピクセルアンパックバッファのバインディング
    std::array<GLint, 6> settings{};    // 現在のピクセルアンパック設定の保存用

    /**
     *  @brief ピクセル展開時に扱う転送設定。
     */
    static constexpr auto kUnpackSettings = std::array
    {
        GL_UNPACK_ALIGNMENT,        // 各行の開始位置をそろえるバイト境界
        GL_UNPACK_ROW_LENGTH,       // 1行あたりのピクセル数
        GL_UNPACK_IMAGE_HEIGHT,     // 1画像あたりの行数
        GL_UNPACK_SKIP_PIXELS,      // 読み飛ばすピクセル数
        GL_UNPACK_SKIP_ROWS,        // 読み飛ばす行数
        GL_UNPACK_SKIP_IMAGES       // 読み飛ばす画像数
    };

    /**
     *  @brief コンストラクタ。現在の転送設定を保存し、CPU の初期値を設定する。
     */
    UploadState()
    {
        // バッファ取得
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        for (auto index = std::size_t{}; index < kUnpackSettings.size(); ++index)
        {
            glGetIntegerv(kUnpackSettings[index], &settings[index]);
            glPixelStorei(kUnpackSettings[index], index == 0 ? 1 : 0);    // アライメントだけ1バイト設定
        }

        // ピクセル転送用のバッファを解除
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    /**
     *  @brief デストラクタ。保存していた転送設定を復元する。
     */
    ~UploadState()
    {
        for (auto index = std::size_t{}; index < kUnpackSettings.size(); ++index)
        {
            glPixelStorei(kUnpackSettings[index], settings[index]);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer));
        glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(texture));
    }
};


}

/** 
 *  @brief OpenGL 資源を保持し、コンテキストが有効な間に解放する。
 */
struct WindGrid::Impl
{
    /** 
    *  @brief タイマーのスロット情報を保持する。 
    */
    struct TimerSlot
    {
        GLuint motor           = 0;     // 発生器寄与計算の GPU 時間クエリ ID
        GLuint update          = 0;     // 風更新の GPU 時間クエリ ID
        bool   motorRecorded   = false; // 発生器寄与計算の計測結果を取得待ちか
        bool   updateRecorded  = false; // 風更新の計測結果を取得待ちか
        bool   pending         = false; // このスロットに未取得の計測結果があるか
    };

    std::array<int, windDefs::kAxisCount> size;                              // 格子の各軸のセル数
    std::array<std::array<GLuint, windDefs::kAxisCount>, 2> textures{};      // 現在・次回の風速テクスチャ
    std::array<GLuint, ProgramIndex(ComputeProgram::Count)> programs{};      // 風計算に使うコンピュートプログラム
    std::unique_ptr<detail::WindMotorGpu> motorGpu;                          // 発生器の GPU 寄与とキャッシュ
    std::array<GLuint, kTimerSlotCount * 2> timerQueries{};                  // 各スロットの GPU 時間計測クエリ
    std::array<TimerSlot, kTimerSlotCount> timerSlots{};                     // GPU 計測結果の取得状態
    WindPerformanceStats performance{};                                      // 最後に取得した処理時間
    std::size_t nextTimerSlot = 0;                                           // 次に計測へ使うスロットの探索開始位置
    int current = 0;                                                         // 現在参照するテクスチャ組の番号
    float distribution = 0.25F;                                              // 60 Hz 基準の風の分配割合
    WindDistribution distributionMethod = WindDistribution::SpeedDependent;  // 風の分配方式
    float halfLife = 2.0F;                                                   // 基本風との差が半分になる時間（秒）

    /**
     *  @brief デストラクタ
     */
    ~Impl()
    {
        // テクスチャの解放
        for (const auto& group : textures)
        {
            glDeleteTextures(static_cast<GLsizei>(windDefs::kAxisCount), group.data());
        }
        
        // プログラムの解放
        for (const auto program : programs) 
        { 
            if (program != 0) { glDeleteProgram(program); }
        }

        // タイマーの解放
        glDeleteQueries(static_cast<GLsizei>(timerQueries.size()), timerQueries.data());
    }

    /**
     *  @brief 対象の大きさに合わせて計算を開始する。 
     */
    void Dispatch() const
    {
        glDispatchCompute(static_cast<GLuint>((size[0] + 3) / 4),
                          static_cast<GLuint>((size[1] + 3) / 4),
                          static_cast<GLuint>((size[2] + 3) / 4));
    }

    /**
     *  @brief 完了済みの GPU タイマーだけを読み、利用可能な枠を返す。 
     */
    std::size_t PrepareTimerSlot()
    {
        // 完了待ちの計測だけを調べ、GPU の処理を待たずに取得できた結果を反映する。
        for (auto index = std::size_t{}; index < timerSlots.size(); ++index)
        {
            // スロットの状態を取得
            auto& slot = timerSlots[index];
            if (!slot.pending) { continue; }

            // 発生器の種別に応じた計測結果を取得する
            auto available = GLint{};
            if (slot.motorRecorded)     // 風発生器
            {
                glGetQueryObjectiv(slot.motor, GL_QUERY_RESULT_AVAILABLE, &available);
                // 結果が未完了ならこの枠を保持し、後の呼び出しで再確認する。
                if (available == GL_FALSE) { continue; }
                auto nanoseconds = GLuint64{};
                glGetQueryObjectui64v(slot.motor, GL_QUERY_RESULT, &nanoseconds);
                performance.gpuMotorMilliseconds = static_cast<double>(nanoseconds) / 1.0e6;
                performance.hasGpuMotor = true;
                slot.motorRecorded = false;
            }
            if (slot.updateRecorded)    // 風更新
            {
                glGetQueryObjectiv(slot.update, GL_QUERY_RESULT_AVAILABLE, &available);
                // 発生器計測と同様に、風更新の計測も完了してから結果を読む。
                if (available == GL_FALSE) { continue; }
                auto nanoseconds = GLuint64{};
                glGetQueryObjectui64v(slot.update, GL_QUERY_RESULT, &nanoseconds);
                performance.gpuStepMilliseconds = static_cast<double>(nanoseconds) / 1.0e6;
                performance.hasGpuStep = true;
                slot.updateRecorded = false;
            }
            // このスロットに記録された計測結果をすべて取得したため、再利用可能にする。
            slot.pending = false;
        }

        // 次の位置から空き枠を巡回して探し、計測クエリを割り当てる。
        for (auto offset = std::size_t{}; offset < timerSlots.size(); ++offset)
        {
            const auto index = (nextTimerSlot + offset) % timerSlots.size();
            if (timerSlots[index].pending) { continue; }
            nextTimerSlot = (index + 1) % timerSlots.size();
            timerSlots[index] = {};
            timerSlots[index].motor = timerQueries[index * 2];
            timerSlots[index].update = timerQueries[index * 2 + 1];
            return index;
        }
        // すべての枠が GPU の完了待ちなら、今回の計測には枠を割り当てない。
        return timerSlots.size();
    }
};


/**
 * @brief WindGrid のコンストラクタ
 * @param i_size        風を管理する範囲
 * @param i_initial     風の初期値
 */
WindGrid::WindGrid(const std::array<int, windDefs::kAxisCount>&     i_size,
                   const std::array<float, windDefs::kAxisCount>&   i_initial)
    : m_instImpl(std::make_unique<Impl>())
{
    ValidateWind(i_initial);

    // 風を管理する空間サイズの妥当性を検証
    auto maximum = GLint{};
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maximum);
    CheckGl("風の格子の上限取得");
    for (const auto value : i_size)
    {
        if (value <= 0 || value > maximum || value > std::numeric_limits<int>::max() - 3)
        {
            throw std::invalid_argument("風の格子の寸法が不正です。");
        }
    }
    m_instImpl->size = i_size;

    // 風の計算用シェーダーをコンパイル
    m_instImpl->programs[ProgramIndex(ComputeProgram::WindClear)]   =   Compile(shaders::windClear);
    m_instImpl->programs[ProgramIndex(ComputeProgram::WindScatter)] =   Compile(shaders::windScatter);
    m_instImpl->programs[ProgramIndex(ComputeProgram::WindFinish)]  =   Compile(shaders::windFinish);
    m_instImpl->motorGpu = std::make_unique<detail::WindMotorGpu>();

    // 風の GPU 計測クエリを作成
    glGenQueries(static_cast<GLsizei>(m_instImpl->timerQueries.size()), m_instImpl->timerQueries.data());
    CheckGl("風の GPU 計測クエリ作成");
    if (std::any_of(
        m_instImpl->timerQueries.begin(),
        m_instImpl->timerQueries.end(),
        [](const auto i_query) {
            return i_query == 0;
        }
    ))
    {
        throw std::runtime_error("風の GPU 計測クエリを作成できません。");
    }

    // 3軸分の風の3Dテクスチャを作成
    const auto cells        =   static_cast<std::size_t>(i_size[0]) * static_cast<std::size_t>(i_size[1]) * static_cast<std::size_t>(i_size[2]);
    const auto uploadState  =   UploadState{};
    for (auto& group : m_instImpl->textures)
    {
        glGenTextures(static_cast<GLsizei>(windDefs::kAxisCount), group.data());
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            if (group[axis] == 0) { throw std::runtime_error("風のテクスチャを作成できません。"); }

            // 軸ごとの初期風速を整数に変換してテクスチャにアップロード
            const auto  value   =   static_cast<std::int32_t>(std::round(i_initial[axis] * kScale));
            const auto  data    =   std::vector<std::int32_t>(cells, value);
            glBindTexture(GL_TEXTURE_3D, group[axis]);

            // 過去と現在の風速を格納するためのピンポンバッファリングするために二枚のテクスチャを用意
            glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32I, i_size[0], i_size[1], i_size[2]);
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, i_size[0], i_size[1], i_size[2], GL_RED_INTEGER, GL_INT, data.data());
            CheckGl("風のテクスチャの初期化");
        }
    }
}

/**
 *  @brief WindGrid のデストラクタ。
 */
WindGrid::~WindGrid() = default;

/**
 *  @brief WindGrid のパフォーマンス統計を取得。
 */
WindPerformanceStats WindGrid::Performance() const
{
    return m_instImpl->performance;
}

/**
 *  @brief  風の分配割合を設定。
 *  @param  i_fraction  0 以上 1 以下の有限値。
 */
void WindGrid::SetDistribution(const float i_fraction)
{
    if (!std::isfinite(i_fraction) || i_fraction < 0.0F || i_fraction > 1.0F)
    {
        throw std::invalid_argument("風の分配割合は 0 以上 1 以下の有限値にしてください。");
    }
    m_instImpl->distribution = i_fraction;
}

/**
 *  @brief  風の分配方式を設定。
 *  @param  i_method 風の分配方式。
 */
void WindGrid::SetDistributionMethod(const WindDistribution i_method)
{
    if (i_method != WindDistribution::Constant && i_method != WindDistribution::SpeedDependent)
    {
        throw std::invalid_argument("風の分配方式が不正です。");
    }
    m_instImpl->distributionMethod = i_method;
}


/**
 *  @brief 風の半減時間を設定。
 *  @param i_seconds 正の有限値。
 */
void WindGrid::SetHalfLife(const float i_seconds)
{
    if (!std::isfinite(i_seconds) || i_seconds <= 0.0F)
    {
        throw std::invalid_argument("風の半減時間は正の有限値にしてください。");
    }
    m_instImpl->halfLife = i_seconds;
}

/**
 *  @brief 管理空間の移動後も重複領域の風を同じワールド座標に保持する。
 *  @param i_offset     管理空間の各軸の移動セル数。
 *  @param i_baseWind   新たに管理範囲へ入るセルの風速（m/s）。
 *  @throws std::invalid_argument 基本風が不正な場合。
 *  @throws std::runtime_error GPU 処理に失敗した場合。
 */
void WindGrid::Move(const std::array<int, windDefs::kAxisCount>&    i_offset,
                    const std::array<float, windDefs::kAxisCount>&  i_baseWind)
{
    ValidateWind(i_baseWind);

    if (i_offset == std::array<int, windDefs::kAxisCount>{}) { return; }
    
    // 移動前の格子を読み取り、もう一方の格子に移動後の風を書き込む。
    auto& impl      =   *m_instImpl;
    const auto next =   1 - impl.current;
    auto offset     =   std::array<int, windDefs::kAxisCount>{};
    auto initial    =   std::array<int, windDefs::kAxisCount>{};
    for (auto axis  = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        // 軸方向の移動が格子の寸法以上なら重複はないため、シェーダー内の加算があふれない範囲に収める。
        offset[axis]    =   std::clamp(i_offset[axis], -impl.size[axis], impl.size[axis]);

        // テクスチャの符号付き 16.16 固定小数点に合わせ、新領域の基本風を変換する。
        initial[axis]   =   static_cast<int>(std::round(i_baseWind[axis] * kScale));
    }
    // 同じワールド座標の値を引き継ぎ、新領域を基本風で埋める移動モードを指定する。
    glUseProgram(impl.programs[ProgramIndex(ComputeProgram::WindClear)]);   // 初期化
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "size"), 1, impl.size.data());
    glUniform1i(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "moveRegion"), 1);
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "offset"), 1, offset.data());
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "initialWind"), 1, initial.data());

    // 軸ごとに移動先を書き込み専用、移動元を読み取り専用として結合する。
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        glBindImageTexture(static_cast<GLuint>(axis), impl.textures[next][axis], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32I);
        glBindImageTexture(static_cast<GLuint>(axis + windDefs::kAxisCount),
                           impl.textures[impl.current][axis], 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32I);
    }
    impl.Dispatch();

    // 後続の画像操作、テクスチャ参照、読み戻しから移動結果が見えるようにする。
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
    CheckGl("風の管理空間の移動");
    
    // エラーがなければ移動先を現在の格子に切り替える。
    impl.current = next;
}

/**
 *  @brief      発生器の寄与を使わず、風を隣接セルへ一度伝播する。
 *  @param      i_seconds   風の更新時間（秒）。正の有限値。
 *  @param      i_baseWind  空間外から流入する風と減衰先の風速（m/s）。大きさは 64 m/s 以下。
 *  @throws     std::invalid_argument 更新時間または基本風が不正な場合。
 *  @throws     std::runtime_error GPU 処理に失敗した場合。
 *  @details    
 *  現在の風を分配し、境界からの流入と基本風への減衰を反映する。
 *  有効な OpenGL 4.3 コンテキストが必要で、プログラムと画像ユニット 0～5 の設定を変更する。
 */
void WindGrid::Step(const float i_seconds, const std::array<float, windDefs::kAxisCount>& i_baseWind)
{
    StepWithMotors(i_seconds, i_baseWind, nullptr);
}

/**
 *  @brief GPU に渡すセルごとの発生器の風。
 */
struct WindGrid::MotorInput
{
    detail::WindMotorInput input;   // GPU に渡すセルごとの発生器の風の入力データ
};

/**
 *  @brief  発生器の寄与を反映し、風を隣接セルへ一度伝播する。
 *  @param  i_seconds 風の更新時間（秒）。正の有限値。
 *  @param  i_baseWind 空間外から流入する風と減衰先の風速（m/s）。大きさは 64 m/s 以下。
 *  @param  i_origin 管理空間の最小ワールド座標（整数メートル）。
 *  @param  io_motors 反映する発生器。更新成功後に瞬間発生器を解放する。
 *  @param  i_settings セル内の占有率を近似する分割数。
 *  @throws std::invalid_argument 更新時間、基本風、管理空間の座標または分割数が不正な場合。
 *  @throws std::runtime_error GPU 処理または瞬間風の完了待ちに失敗した場合。
 *  @details
 *  発生器の一覧と管理空間の設定が変わらなければ、GPU 上の寄与を再利用する。
 *  瞬間発生器がある場合は GPU の完了を待ってから解放する。
 *  呼び出し中に同じ発生器管理器を他のスレッドから変更してはならない。
 *  有効な OpenGL 4.3 コンテキストが必要で、プログラム、画像ユニット 0～5 と
 *  シェーダーストレージの結合点 0～1 を変更する。
 */
void WindGrid::Step(const float                                     i_seconds,
                    const std::array<float, windDefs::kAxisCount>&  i_baseWind,
                    const std::array<int, windDefs::kAxisCount>&    i_origin,
                    motor::WindMotors&                              io_motors,
                    const OccupancySettings&                        i_settings)
{
    // 不正な入力によってキャッシュや発生器の状態を変更しないよう、先に検証する。
    ValidateWind(i_baseWind);
    if (!std::isfinite(i_seconds) || i_seconds <= 0.0F)
    {
        throw std::invalid_argument("風の更新時間は正の有限値にしてください。");
    }

    auto& impl = *m_instImpl;
    ValidateOrigin(i_origin, impl.size);
    if (i_settings.subdivisions < defs::kMinimumSubdivisions ||
        i_settings.subdivisions > defs::kMaximumSubdivisions)
    {
        throw std::invalid_argument("占有率の分割数が不正です。");
    }

    // 現在の発生器と管理空間の設定を前回の入力と比較し、GPU 上の寄与を再利用できるか決める。
    auto input = MotorInput{impl.motorGpu->Prepare(io_motors.Snapshot(), i_origin, impl.size,
                                                   i_settings.subdivisions)};
    impl.performance.motorCacheRebuilt = input.input.rebuilt;

    // 更新に失敗した場合は瞬間発生器を残し、再生成した入力もキャッシュに確定しない。
    StepWithMotors(i_seconds, i_baseWind, &input);
    impl.motorGpu->Complete(std::move(input.input), io_motors);
}

/**
 *  @brief  必要に応じて発生器の寄与を準備し、GPU 上で風を一度更新する。
 *  @param  i_seconds   風の更新時間（秒）。正の有限値。
 *  @param  i_baseWind  空間外から流入する風と減衰先の風速（m/s）。大きさは 64 m/s 以下。
 *  @param  i_input     発生器の入力。発生器を反映しない場合は null。
 *  @throws std::invalid_argument 更新時間または基本風が不正な場合。
 *  @throws std::runtime_error GPU 処理または瞬間風の完了待ちに失敗した場合。
 *  @details
 *  瞬間発生器がある場合は GPU の完了を待つが、発生器の解放は呼び出し側が行う。
 *  成功時に更新先のテクスチャを現在の格子へ切り替える。
 */
void WindGrid::StepWithMotors(const float                                       i_seconds,
                              const std::array<float, windDefs::kAxisCount>&    i_baseWind,
                              const MotorInput*                                 i_input)
{
    // GPU 処理を始める前に、両方の Step から渡される更新時間と基本風を検証する。
    ValidateWind(i_baseWind);
    if (!std::isfinite(i_seconds) || i_seconds <= 0.0F)
    {
        throw std::invalid_argument("風の更新時間は正の有限値にしてください。");
    }

    // 未完了の計測を待たずに空き枠を探し、枠がなければ今回の計測だけを省く。
    auto& impl              =   *m_instImpl;
    const auto timerIndex   =   impl.PrepareTimerSlot();
    auto* timer             =   timerIndex < impl.timerSlots.size() ? &impl.timerSlots[timerIndex] : nullptr;

    // 現在のテクスチャは読み取り専用にし、もう一方を今回の更新先にする。
    const auto next             =   1 - impl.current;
    const auto speedDependent   =   impl.distributionMethod == WindDistribution::SpeedDependent;

    // 60 Hz 基準の分配割合を今回の時間に換算する。
    // 微小な割合も消えにくいよう対数と指数を使う。
    const auto portion = static_cast<float>(-std::expm1(std::log1p(-static_cast<double>(impl.distribution)) *
                                                         static_cast<double>(i_seconds) * 60.0));

    // 風速比例方式では、境界から流入する基本風にもその風速に応じた分配割合を適用する。
    const auto baseSpeed = std::hypot(static_cast<double>(i_baseWind[windDefs::kAxisXIndex]),
                                      static_cast<double>(i_baseWind[windDefs::kAxisYIndex]),
                                      static_cast<double>(i_baseWind[windDefs::kAxisZIndex]));
    const auto baseFraction = static_cast<double>(impl.distribution) * std::min(baseSpeed / 64.0, 1.0);
    const auto basePortion = speedDependent
        ? static_cast<float>(-std::expm1(std::log1p(-baseFraction) * static_cast<double>(i_seconds) * 60.0))
        : portion;

    // 更新時間と半減時間から、現在の風を目標風に近づける割合を求める。
    const auto blend = static_cast<float>(-std::expm1(-std::log(2.0) * static_cast<double>(i_seconds) /
                                                      static_cast<double>(impl.halfLife)));

    // 発生器の入力が変わった場合はセル別寄与を再生成し、変わらなければ既存の寄与を結合する。
    if (i_input != nullptr)
    {
        impl.motorGpu->Apply(i_input->input, timer == nullptr ? 0 : timer->motor,
                             timer == nullptr ? nullptr : &timer->motorRecorded,
                             timer == nullptr ? nullptr : &timer->pending);
    }

    // 次の格子の消去から仕上げまでを GPU 時間の計測対象にする。
    auto updateQuery = ElapsedQuery(timer == nullptr ? 0 : timer->update);

    // 前回の値が分配結果に混ざらないよう、更新先の各成分をゼロにする。
    glUseProgram(impl.programs[ProgramIndex(ComputeProgram::WindClear)]);
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "size"), 1, impl.size.data());
    glUniform1i(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindClear)], "moveRegion"), 0);
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        glBindImageTexture(static_cast<GLuint>(axis), impl.textures[next][axis], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32I);
    }
    impl.Dispatch();

    // 消去結果を分配処理から参照できるようにする。
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 現在の格子から風を読み、風向きに応じて更新先の隣接セルへ加算する。
    glUseProgram(impl.programs[ProgramIndex(ComputeProgram::WindScatter)]);
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindScatter)], "size"), 1, impl.size.data());
    glUniform1f(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindScatter)], "portion"), portion);
    glUniform1i(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindScatter)], "speedDependent"),
                speedDependent ? 1 : 0);
    glUniform1f(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindScatter)], "fraction60"),
                impl.distribution);
    glUniform1f(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindScatter)], "seconds"), i_seconds);
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        glBindImageTexture(static_cast<GLuint>(axis), impl.textures[impl.current][axis], 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32I);
        glBindImageTexture(static_cast<GLuint>(axis + windDefs::kAxisCount),
                           impl.textures[next][axis], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32I);
    }
    impl.Dispatch();

    // 全ての分配結果を仕上げ処理から参照できるようにする。
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 境界からの流入、目標風への減衰、必要に応じた発生器の寄与を反映する。
    glUseProgram(impl.programs[ProgramIndex(ComputeProgram::WindFinish)]);
    glUniform3iv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindFinish)], "size"), 1,
                 impl.size.data());
    glUniform3fv(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindFinish)], "baseWind"), 1,
                 i_baseWind.data());
    glUniform1f(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindFinish)], "portion"), basePortion);
    glUniform1f(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindFinish)], "blend"), blend);
    glUniform1i(Uniform(impl.programs[ProgramIndex(ComputeProgram::WindFinish)], "hasMotors"),
                i_input != nullptr ? 1 : 0);
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        glBindImageTexture(static_cast<GLuint>(axis), impl.textures[next][axis], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32I);
    }
    impl.Dispatch();

    // 後続の画像操作、描画時のテクスチャ参照、CPU への読み戻しから更新結果が見えるようにする。
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_TEXTURE_UPDATE_BARRIER_BIT);
    updateQuery.End();

    // 計測結果はここでは待たず、次回以降に取得する。
    if (timer != nullptr)
    {
        timer->updateRecorded = true;
        timer->pending = true;
    }
    CheckGl("風の更新");

    // 瞬間発生器は一度だけ反映するため、呼び出し側が解放する前に GPU の完了を確認する。
    if (i_input != nullptr &&
        std::any_of(i_input->input.key.motors.begin(),
                    i_input->input.key.motors.end(),
                    [](const auto& i_motor) {
                        return i_motor.settings.mode == motor::Mode::Instant;
                    }
                )
        )
    {
        // 瞬間発生器の更新が GPU 上で完了するのを待つ
        const auto sync =   glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (sync == nullptr) { throw std::runtime_error("風の更新完了を確認できません。"); }

        // フェンスの終了を待つ
        const auto status = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10000000000ULL);
        glDeleteSync(sync);
        if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
        {
            CheckGl("風の更新完了待ち");
            throw std::runtime_error("風の更新完了を待てませんでした。");
        }
        CheckGl("風の更新完了待ち");
    }

    // 更新と必要な完了確認が成功した場合だけ、更新先を現在の格子に切り替える。
    impl.current = next;
}

/**
 * @brief 現在の風グリッドのテクスチャを取得する。
 *
 * @return std::array<unsigned int, windDefs::kAxisCount>   現在の風グリッドの各軸のテクスチャID。
 */
std::array<unsigned int, windDefs::kAxisCount> WindGrid::Textures() const
{
    return m_instImpl->textures[m_instImpl->current];
}

}
