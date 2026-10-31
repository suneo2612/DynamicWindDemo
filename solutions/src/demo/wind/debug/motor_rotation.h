/**
 * @file motor_rotation.h
 * @brief デバッグ発生器の自動回転を定義する。
 * @date 2026-11-01
 * @par 最終更新日 2026-11-01
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_MOTOR_ROTATION_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_MOTOR_ROTATION_H

#include <array>

namespace demo::wind::debug
{
/**
 * @brief ワールド座標の固定軸を中心とする自動回転の設定。
 */
struct MotorRotation
{
    bool enabled = false;                         // 自動回転を使うか
    std::array<float, 3> axis{0.0F, 1.0F, 0.0F}; // ワールド座標の回転軸
    float degreesPerSecond = 45.0F;               // 右手系の角速度（度/秒）
};

/**
 * @brief 自動回転の設定を検証する。
 * @param i_rotation 検証する設定。
 * @throws std::invalid_argument 軸や角速度が非有限、または軸がゼロの場合。
 */
void ValidateMotorRotation(const MotorRotation& i_rotation);

/**
 * @brief 方向を固定軸の周りに回転させる。
 * @param i_direction 回転する方向。ゼロでない有限ベクトル。
 * @param i_rotation 回転設定。
 * @param i_elapsed 経過秒数。0 以上の有限値。
 * @return 回転後の単位方向。回転を行わない場合は入力方向をそのまま返す。
 * @throws std::invalid_argument 設定、方向または経過時間が不正な場合。
 */
std::array<float, 3> RotateMotorDirection(const std::array<float, 3>& i_direction,
                                          const MotorRotation& i_rotation,
                                          double i_elapsed);
}
#endif
