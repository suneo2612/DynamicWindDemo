/**
 * @file camera_constants.h
 * @brief カメラの初期姿勢と操作・投影に使う定数を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_RENDER_DEFS_CAMERA_CONSTANTS_H
#define DYNAMIC_WIND_DEMO_RENDER_DEFS_CAMERA_CONSTANTS_H

namespace render::defs
{
// カメラの初期姿勢と操作・投影に使う定数
inline constexpr float kCameraInitialPositionX      =   0.0F;             // 初期位置の X 座標
inline constexpr float kCameraInitialPositionY      =   3.0F;            // 初期位置の Y 座標
inline constexpr float kCameraInitialPositionZ      =   10.0F;            // 初期位置の Z 座標
inline constexpr float kCameraInitialYawRadians     =   -1.57079632679F;  // 初期視線を負の Z 方向に向けるヨー角
inline constexpr float kCameraInitialPitchRadians   =   0.0F;             // 初期視線を水平にするピッチ角

// カメラの回転に関する定数
inline constexpr float kCameraRotationRadiansPerPixel   =   0.003F;          // マウス1ピクセルあたりの回転角
inline constexpr float kCameraFullTurnRadians           =   6.28318530718F;  // ヨー角を折り返す1回転分の角度
inline constexpr float kCameraMaxPitchRadians           =   1.55F;           // 真上・真下付近を避けるピッチ角の上限

// カメラの投影に関する定数
inline constexpr float kCameraNearPlane             =   0.01F;            // 透視投影の近クリップ面までの距離
inline constexpr float kCameraFarPlane              =   10000.0F;         // 透視投影の遠クリップ面までの距離
inline constexpr float kCameraVerticalFovRadians    =   0.78539816339F;   // 透視投影の垂直視野角
}
#endif
