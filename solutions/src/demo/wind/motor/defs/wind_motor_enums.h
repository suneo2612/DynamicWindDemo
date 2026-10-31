/**
 * @file wind_motor_enums.h
 * @brief 風発生器で共有する列挙型を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_DEFS_WIND_MOTOR_ENUMS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_DEFS_WIND_MOTOR_ENUMS_H

namespace demo::wind::motor
{

/**
 * @brief 発生器の風が届く範囲の形状。
 */
enum class Shape
{
    Sphere,     // 球
    Cone,       // 円錐
    Cylinder    // 円柱
};

/**
 * @brief 風を出し続けるか、一度だけ出すかを表す。
 */
enum class Mode
{
    Continuous, // 連続風発生
    Instant     // 瞬間風発生
};

/**
 * @brief 生成要求の処理状態。
 */
enum class RequestStatus
{
    Pending,    // 処理待ち
    Succeeded,  // 生成成功
    Failed      // 生成失敗
};

}
#endif
