/**
 * @file marching_cubes.h
 * @brief 立方体内の境界面と占有体積を求める内部処理を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_MARCHING_CUBES_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_MARCHING_CUBES_H

#include "defs/marching_cube_constraints.h"
#include <array>

namespace demo::wind::simulation::detail
{

/**
 * @brief   単位立方体に対するマーチングキューブの近似体積を求める。
 * @param   i_values 8 頂点の境界値。0 以下が内側。頂点番号は X、Y、Z を下位から各 1 ビットに対応させる。
 * @param   i_faceValues 各面の中心の境界値。順番は X 最小、X 最大、Y 最小、Y 最大、Z 最小、Z 最大。
 * @return  閉じた近似領域の体積。0 以上 1 以下。
 * @details 
 * 辺の両端の境界値を線形補間して交点を求める。面の対角に内側の頂点がある場合は、
 * 面の中心が内側か外側かで交点の接続を決める。同じ面を共有する立方体では同じ中心値が必要。
 * @throws  std::logic_error 境界面を閉じることができなかった場合。
 */
double MarchingCubeVolume(const std::array<double, defs::kVertexCount>& i_values,
                          const std::array<double, defs::kFaceCount>& i_faceValues);
}
#endif
