/**
 * @file wind_motor_gpu.h
 * @brief 風発生器の GPU 寄与と入力キャッシュを管理する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_MOTOR_GPU_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_MOTOR_GPU_H

#include "occupancy.h"
#include <GL/glew.h>
#include <array>
#include <optional>
#include <vector>

namespace demo::wind::simulation::detail
{
/**
 * @brief GPU に保持された寄与の入力。
 */
struct MotorCacheKey
{
    std::vector<motor::Motor>                       motors;         // 発生器の一覧。
    std::array<int, demo::wind::defs::kAxisCount>   origin;         // 管理空間の原点。
    std::array<int, demo::wind::defs::kAxisCount>   size;           // 管理空間の寸法。
    int                                             subdivisions;   // 占有率の分割数。
    bool operator==(const MotorCacheKey& i_other) const = default;  // 全てのメンバーを比較
};

/**
 * @brief 風の一回の更新に使用する発生器の入力。
 */
struct WindMotorInput
{
    MotorCacheKey   key;        // 更新開始時の発生器と管理空間。
    bool            rebuilt;    // GPU 上の寄与を再生成するか。
};

/**
 * @brief GPU の発生器寄与とその再利用条件を所有する。
 * @details 作成、使用、破棄は同じ有効な OpenGL コンテキスト上で行う。
 */
class WindMotorGpu final
{
public:
    /**
     * @brief 発生器シェーダーのプログラムを作成する。
     * @throws std::runtime_error シェーダーの作成、コンパイルまたはリンクに失敗した場合。
     */
    WindMotorGpu();
    ~WindMotorGpu();
    WindMotorGpu(const WindMotorGpu&) = delete;
    WindMotorGpu& operator=(const WindMotorGpu&) = delete;

    /**
     * @brief 入力を比較し、再生成が必要な場合は古いキャッシュを破棄する。
     * @param i_motors 発生器のスナップショット。
     * @param i_origin 管理空間の最小ワールド座標（整数メートル）。
     * @param i_size 格子の XYZ 各軸のセル数。
     * @param i_subdivisions 占有率の分割数。
     * @return 今回の更新に用いる入力と再計算の要否。
     * @details 入力が変わった場合は旧キャッシュを破棄する。新しい入力の確定は Complete() が行う。
     * @pre 引数は呼び出し側で検証済みとし、発生器の方向は正規化可能とする。
     */
    WindMotorInput Prepare(std::vector<motor::Motor> i_motors,
                           const std::array<int, demo::wind::defs::kAxisCount>& i_origin,
                           const std::array<int, demo::wind::defs::kAxisCount>& i_size,
                           int i_subdivisions);

    /**
     * @brief 必要なら寄与を再生成し、風更新用にバッファーを結合する。
     * @param i_input 今回の発生器入力。
     * @param i_query GPU 時間クエリ。計測しない場合は 0。
     * @param o_recorded 再計算を実行した場合に true を設定する記録状態。使用しない場合は null。
     * @param o_pending 再計算を実行した場合に true を設定する結果の取得待ち状態。使用しない場合は null。
     * @throws std::runtime_error GPU の容量上限を超えた場合、または GPU 処理に失敗した場合。
     * @details シェーダーストレージの結合点 0 を変更し、再計算時は結合点 1 とプログラムも変更する。
     * @pre o_recorded が null でない場合、o_pending も有効なポインターとする。
     */
    void Apply(const WindMotorInput& i_input, GLuint i_query, bool* o_recorded, bool* o_pending);

    /**
     * @brief 更新完了後に瞬間発生器を解放し、再生成した入力を確定する。
     * @param io_input 今回の更新に使用した入力。再計算した場合はキャッシュへ移動する。
     * @param io_motors 瞬間発生器を解放する管理器。
     * @pre 風の更新が成功し、瞬間発生器がある場合は GPU の更新完了を確認済みとする。
     * @pre 更新中に同じ発生器管理器を他のスレッドから変更しない。
     */
    void Complete(WindMotorInput&& io_input, motor::WindMotors& io_motors);

private:
    GLuint m_uProgram = 0;                      // 発生器寄与の計算プログラム。
    GLuint m_uMotorBuffer = 0;                  // セルごとの寄与。
    GLuint m_uSettingsBuffer = 0;               // 発生器設定。
    std::optional<MotorCacheKey> m_instCache;   // 更新成功時に確定した入力。
};

}   // namespace demo::wind::simulation::detail
#endif
