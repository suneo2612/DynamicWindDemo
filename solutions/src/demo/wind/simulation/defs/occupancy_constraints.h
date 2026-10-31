/**
 * @file occupancy_constraints.h
 * @brief セルの占有率を近似するときの分割数を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_DEFS_OCCUPANCY_CONSTRAINTS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_DEFS_OCCUPANCY_CONSTRAINTS_H

namespace demo::wind::simulation::defs
{
inline constexpr auto kMinimumSubdivisions = 1;  // 各辺の分割数の下限
inline constexpr auto kMaximumSubdivisions = 4;  // 各辺の分割数の上限
inline constexpr auto kDefaultSubdivisions = 4;  // 指定がない場合の各辺の分割数
}
#endif
