/**
 * @file wind_debug_geometry.h
 * @brief 風の表示用線分を生成する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_GEOMETRY_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_GEOMETRY_H

#include "../defs/wind_region_constants.h"
#include "../motor/wind_motor.h"
#include <array>
#include <vector>

namespace demo::wind::debug
{

/**
 * @brief 発生器の形状を、2 頂点ずつの線分に変換する。
 * @param i_settings 描画する発生器の設定。位置と方向はワールド座標。
 * @return 線分の両端の座標。
 * @throws std::invalid_argument 形状、寸法、位置、方向が不正、または頂点座標を float で表現できない場合。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildWireframe(const motor::Settings& i_settings);

/**
 * @brief ワールド軸に沿った直方体の管理空間を線分へ変換する。
 * @param i_origin 管理空間の最小 XYZ 座標。
 * @param i_size XYZ 各軸のセル数。
 * @return 直方体を構成する 12 本の線分。
 * @throws std::invalid_argument セル数が正でない、または頂点座標を float で表現できない場合。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildRegionWireframe(
    const std::array<int, defs::kAxisCount>& i_origin,
    const std::array<int, defs::kAxisCount>& i_size);

/**
 * @brief 管理空間内の全セルの辺を重複しない格子線に変換する。
 * @param i_origin 管理空間の最小 XYZ 座標（m）。
 * @param i_size XYZ 各軸のセル数。各セルの辺は 1 m。
 * @return 軸ごとに管理空間の端から端まで結んだ線分。
 * @throws std::invalid_argument セル数が正でない、または頂点座標を float で表現できない場合。
 * @throws std::overflow_error 1 m 単位の座標を表現できない、または線分数が OpenGL の上限を超える場合。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildCellGridWireframe(
    const std::array<int, defs::kAxisCount>& i_origin,
    const std::array<int, defs::kAxisCount>& i_size);
}
#endif
