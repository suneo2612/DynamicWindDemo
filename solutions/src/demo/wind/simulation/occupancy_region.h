/**
 * @file occupancy_region.h
 * @brief 発生器とセル領域の保守的な内外判定を定義する。
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_OCCUPANCY_REGION_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_OCCUPANCY_REGION_H

#include "../motor/wind_motor.h"
#include <array>

namespace demo::wind::simulation::detail
{

/** @brief 発生器と直方体領域の関係。 */
enum class RegionRelation
{
    Outside,
    Inside,
    Partial
};

/** @brief 検証済みの発生器形状を使い、直方体領域を保守的に分類する。 */
class ShapeRegion final
{
public:
    /**
     * @brief 形状を検証し、分類に使う値を準備する。
     * @param i_motor 分類対象の発生器。
     * @throws std::invalid_argument 形状、寸法または方向が不正な場合。
     */
    explicit ShapeRegion(const motor::Settings& i_motor);

    /**
     * @brief 領域が確実に外側または内側か判定する。
     * @param i_min 領域の最小座標。
     * @param i_max 領域の最大座標。
     * @return 確実に分類できない場合は Partial。
     */
    RegionRelation Classify(const std::array<double, 3>& i_min,
                            const std::array<double, 3>& i_max) const;

private:
    motor::Shape m_eShape;
    std::array<double, 3> m_stPosition;
    std::array<double, 3> m_stDirection;
    double m_dRadius;
    double m_dLength;
};
}
#endif
