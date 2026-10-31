/**
 * @file motor_field.h
 * @brief 発生器の形状から各セルに加える風を準備する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_MOTOR_FIELD_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_MOTOR_FIELD_H

#include "occupancy.h"
#include <array>
#include <optional>
#include <vector>

namespace demo::wind::simulation
{

/**
 * @brief 1 セルの発生器の寄与。配列は GPU の vec4 と同じ並び。
 */
struct MotorCell
{
    std::array<float, 4> continuous{}; // 継続風の合計 XYZ と個々の寄与の最大風速
    std::array<float, 4> instant{};    // 瞬間風の合計 XYZ
};

/**
 * @brief 管理空間に対して準備した発生器の風。
 */
struct MotorField
{
    std::vector<MotorCell>       cells;      // X が最も速く変化する順序のセル
    std::vector<motor::MotorId>  instantIds; // 更新成功後に解放する瞬間発生器
};

/**
 * @brief 発生器の一覧から、占有率と距離減衰を考慮した各セルの風を準備する。
 * @param i_motors 有効な発生器の一覧。通常は WindMotors::Snapshot() の結果。
 * @param i_origin 管理空間の最小座標。各軸とも整数メートル。
 * @param i_size X、Y、Z 順のセル数。
 * @param i_settings 占有率の近似精度。
 * @return 各セルの寄与と、管理空間外も含む瞬間発生器の識別子。
 * @throws std::invalid_argument 寸法、座標、発生器または占有率設定が不正な場合。
 * @throws std::overflow_error セル数が表現範囲を超えた場合。
 * @details 方向と距離減衰はセル中心で評価する。占有率があっても中心が形状の外にある場合、
 * 距離減衰は 0～1 に制限する。発生器の解放は行わない。
 */
MotorField BuildMotorField(const std::vector<motor::Motor>& i_motors,
                           const std::array<int, 3>& i_origin,
                           const std::array<int, 3>& i_size,
                           const OccupancySettings& i_settings = {});

/**
 * @brief 発生器と管理空間が変わらない間、セルごとの寄与を再利用する。
 */
class MotorFieldCache final
{
public:
    /**
     * @brief キャッシュ済みの寄与と、今回再計算したかを表す。
     */
    struct Result
    {
        const MotorField& field;    // キャッシュ済みの寄与
        bool rebuilt;               // 今回再計算したかどうか
    };

    /**
     * @brief 入力が変わった場合に計算し、それ以外は前回の寄与を返す。
     * @param i_motors 発生器の一覧。
     * @param i_origin 管理空間の最小座標。
     * @param i_size 管理空間の寸法。
     * @param i_settings 占有率の近似精度。
     * @return 寄与への参照と再計算の有無。参照は次の再計算または破棄まで有効。
     * @throws std::invalid_argument 入力が不正な場合。
     * @throws std::overflow_error セル数が表現範囲を超えた場合。
     */
    Result Get(std::vector<motor::Motor> i_motors,
               const std::array<int, 3>& i_origin,
               const std::array<int, 3>& i_size,
               const OccupancySettings& i_settings);

private:
    /**
     * @brief 再利用判定に使う入力と、その入力から計算した結果を保持する。
     */
    struct Entry
    {
        std::vector<motor::Motor> motors;       // 計算時の発生器一覧
        std::array<int, 3>        origin;       // 計算時の管理空間の最小座標
        std::array<int, 3>        size;         // 計算時の各軸のセル数
        int                       subdivisions; // 占有率を近似する際の各辺の分割数
        MotorField                field;        // 入力から求めた各セルの風と瞬間発生器の識別子
    };

    /**
     * @brief 保存済み結果の入力条件が今回の入力と一致するか判定する。
     * @param i_motors 発生器の一覧。
     * @param i_origin 管理空間の最小座標。
     * @param i_size 管理空間の各軸のセル数。
     * @param i_settings 占有率の近似設定。
     * @return キャッシュが存在し、すべての入力条件が一致する場合は true。
     */
    bool HasMatchingInput(const std::vector<motor::Motor>& i_motors,
                          const std::array<int, 3>& i_origin,
                          const std::array<int, 3>& i_size,
                          const OccupancySettings& i_settings) const;

    std::optional<Entry> m_instEntry; // 前回の計算結果。未計算の場合は値を持たない。
};
}
#endif
