/**
 * @file occupancy.h
 * @brief 風発生器とセルが重なる割合の近似計算を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_OCCUPANCY_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_OCCUPANCY_H

#include "defs/occupancy_constraints.h"
#include "../defs/wind_region_constants.h"
#include "../motor/wind_motor.h"
#include <array>

namespace demo::wind::simulation
{

/**
 * @brief   セルの重なりを近似するときの設定。
 * @details CPU の CalculateOccupancy では各立方体にマーチングキューブ法を適用する。
 * GPU の風更新では小立方体の中心の内外を標本とするため、結果が異なる。
 */
struct OccupancySettings
{
    int subdivisions = defs::kDefaultSubdivisions;  // 各辺の分割数。1 以上 4 以下。
};

/**
 * @brief   一辺 1 m のセルと風発生器の重なりを求める。
 * @param   i_motor 風発生器の設定。無効状態や風速は形状の判定に影響しない。
 * @param   i_cellMin セルの最小座標。ワールド軸に沿った位置（m）。
 * @param   i_settings 境界付近の近似精度。
 * @return  重なった体積の割合。0 以上 1 以下。
 * @throws  std::invalid_argument 形状、寸法、方向、座標または分割数が不正な場合。
 * @throws  std::logic_error 境界面を閉じられない、または計算した体積が立方体の範囲外の場合。
 * @details
 * セルの全頂点が形状内にある場合は 1 を返す。それ以外は小立方体ごとに、
 * 辺の交点をつないだ境界面と立方体の外面で囲まれた体積を足し合わせる。
 * 頂点に現れない重なりは辺・面・立方体の中央も調べ、見つかれば一段階細分化する。
 * それらの位置にも現れない細い重なりは見逃す場合がある。
 * 対角の内側を持つ面は面の中心で接続方法を決め、複数の境界輪郭はそれぞれ独立に閉じる。
 */
float CalculateOccupancy(const motor::Settings&                                 i_motor,
                         const std::array<float, demo::wind::defs::kAxisCount>& i_cellMin,
                         const OccupancySettings&                               i_settings = {});
}   // namespace demo::wind::simulation
#endif
