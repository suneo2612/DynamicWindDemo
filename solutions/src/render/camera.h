/**
 * @file camera.h
 * @brief 視線方向に移動できるカメラを定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_RENDER_CAMERA_H
#define DYNAMIC_WIND_DEMO_RENDER_CAMERA_H

#include "../general/vec/Vector3.h"
#include "defs/camera_constants.h"
#include <array>

namespace render
{
/** @brief 右手系の透視カメラ。角度はラジアン。 */
class Camera final
{
public:
    /** @brief 位置 (0, 10, 10) から負の Z 方向を見るカメラを作る。 */
    Camera() noexcept = default;

    /**
     * @brief 初期位置を指定し、負の Z 方向を見るカメラを作る。
     * @param i_position 初期位置。
     * @throws std::invalid_argument 位置に有限でない値が含まれる場合。
     */
    explicit Camera(const general::vec::Vector3& i_position);

    /**
     * @brief マウス移動量で視線を変更する。
     * @param i_dx 水平方向の移動量。
     * @param i_dy 垂直方向の移動量。
     */
    void Rotate(float i_dx, float i_dy);

    /**
     * @brief 視線方向と右方向に移動する。
     * @param i_forward 前方移動量。
     * @param i_right 右方移動量。
     */
    void Move(float i_forward, float i_right);

    /** @brief 正規化された視線方向を返す。 */
    general::vec::Vector3 Forward() const noexcept;
    /** @brief 現在位置を返す。 */
    general::vec::Vector3 Position() const noexcept { return m_instPosition; }
    /** @brief 列優先のビュー行列を返す。 */
    std::array<float, 16> View() const;

    /**
     * @brief 列優先の透視投影行列を返す。
     * @param i_aspect 横幅と高さの比率。
     * @return 透視投影行列。
     * @throws std::invalid_argument 比率が有限の正数でない場合。
     */
    static std::array<float, 16> Projection(float i_aspect);

private:
    general::vec::Vector3 m_instPosition{
        defs::kCameraInitialPositionX,
        defs::kCameraInitialPositionY,
        defs::kCameraInitialPositionZ};
    float m_fYaw    =   defs::kCameraInitialYawRadians;
    float m_fPitch  =   defs::kCameraInitialPitchRadians;
};
}
#endif
