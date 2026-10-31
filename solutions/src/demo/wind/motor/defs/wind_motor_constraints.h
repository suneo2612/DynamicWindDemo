/**
 * @file wind_motor_constraints.h
 * @brief 風発生器の設定を検証・補正するための制約値を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_DEFS_WIND_MOTOR_CONSTRAINTS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_DEFS_WIND_MOTOR_CONSTRAINTS_H

#include <cstddef>

namespace demo::wind::motor::defs
{

/**
 * @brief 風発生器の直径と、球以外の形状の長さに適用する最小値（m）。
 */
inline constexpr auto kMinimumSize = 1.0F;

/**
 * @brief 風発生器に設定できる風速の上限（m/s）。
 */
inline constexpr auto kMaximumSpeed = 64.0F;

/**
 * @brief 負の風速を補正するときに適用する最小値（m/s）。
 */
inline constexpr auto kMinimumSpeed = 0.0F;

/**
 * @brief 明示指定がない場合にデバッグ専用枠へ割り当てる発生器数。
 */
inline constexpr auto kDefaultDebugCapacity = std::size_t{16};

}
#endif