/**
 * @file wind_region_constants.h
 * @brief 風の管理空間で共有する寸法・初期値・計算定数を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEFS_WIND_REGION_CONSTANTS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEFS_WIND_REGION_CONSTANTS_H

#include <array>
#include <cstddef>
#include <numbers>

namespace demo::wind::defs
{
// 座標配列の要素数と添字。水平視線は Y 軸を除く X・Z の 2 成分で表す。
inline constexpr auto kAxisCount                  = std::size_t{3}; // 位置・寸法などの XYZ 配列長
inline constexpr auto kHorizontalDirectionCount   = std::size_t{2}; // 水平視線ベクトルの要素数

inline constexpr auto kAxisXIndex                 = std::size_t{0}; // XYZ 配列の X 軸要素
inline constexpr auto kAxisYIndex                 = std::size_t{1}; // XYZ 配列の Y 軸要素
inline constexpr auto kAxisZIndex                 = std::size_t{2}; // XYZ 配列の Z 軸要素
inline constexpr auto kHorizontalXIndex           = std::size_t{0}; // 水平ベクトルの X 軸要素
inline constexpr auto kHorizontalZIndex           = std::size_t{1}; // 水平ベクトルの Z 軸要素

// 管理空間設定の初期値と、設定値を検証するための最小寸法。
inline constexpr auto kDefaultSize                = std::array<int, kAxisCount>{64, 32, 64}; // 既定の XYZ セル数
inline constexpr auto kMinimumSize                = std::array<int, kAxisCount>{32, 16, 32}; // 許容する最小の XYZ セル数
inline constexpr auto kDefaultReference           = std::array<float, kAxisCount>{32.0F, 8.0F, 16.0F}; // 既定の基準位置（XYZ）

// 基準点の移動・視線回転に応じて管理空間を更新する既定の検出閾値。
inline constexpr float kDefaultForwardThreshold   = 16.0F; // 前方への移動距離
inline constexpr float kDefaultBackwardThreshold  = 4.0F;  // 後方への移動距離
inline constexpr float kDefaultLateralThreshold   = 16.0F; // 横方向への移動距離
inline constexpr float kDefaultVerticalThreshold  = 2.0F;  // 上下方向への移動距離
inline constexpr float kDefaultRotationDegrees    = 15.0F; // 水平視線の回転角度（度）

// 位置・視線の計算と、設定値や方向ベクトルの検証に共用する数値。
inline constexpr float  kZero                     = 0.0F;                       // ゼロ値との比較・初期化
inline constexpr float  kPositiveUnit             = 1.0F;                       // 右手系の符号
inline constexpr float  kNegativeUnit             = -1.0F;                      // 左手系の符号
inline constexpr double kMinimumHorizontalLength  = 1.0e-6;                     // 水平視線の正規化判定
inline constexpr float  kMinimumCosine            = -1.0F;                      // 余弦値の下限
inline constexpr float  kMaximumCosine            = 1.0F;                       // 余弦値の上限
inline constexpr float  kDegreesPerHalfTurn       = 180.0F;                     // 度からラジアンへの変換係数
inline constexpr float  kHalf                     = 0.5F;                       // 管理空間の中心位置の計算
inline constexpr auto   kPi                       = std::numbers::pi_v<float>;  // ラジアン変換・回転判定
}
#endif
