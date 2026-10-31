/**
 * @file camera.cpp
 * @brief カメラの姿勢、移動と透視投影を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#include "camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace render
{

/**
 * @brief 初期位置を指定し、負の Z 方向を見るカメラを作る。
 * @param i_position 初期位置。
 * @throws std::invalid_argument 位置に有限でない値が含まれる場合。
 */
Camera::Camera(const general::vec::Vector3& i_position) : m_instPosition(i_position)
{
    if (!std::isfinite(i_position.x) || !std::isfinite(i_position.y) || !std::isfinite(i_position.z))
    {
        throw std::invalid_argument("カメラの初期位置は有限値で指定してください。");
    }
}

/**
 * @brief マウス移動量で視線を変更する。
 * @param i_dx 水平方向の移動量。
 * @param i_dy 垂直方向の移動量。
 */
void Camera::Rotate(const float i_dx, const float i_dy)
{
    if (!std::isfinite(i_dx) || !std::isfinite(i_dy))
    {
        throw std::invalid_argument("カメラの回転量は有限値で指定してください。");
    }

    // 横方向の回転（ヨー角）
    m_fYaw = std::remainder(
        m_fYaw + i_dx * defs::kCameraRotationRadiansPerPixel,
        defs::kCameraFullTurnRadians);

    // 縦方向の回転（ピッチ角）
    m_fPitch = std::clamp(
        m_fPitch - i_dy * defs::kCameraRotationRadiansPerPixel,
        -defs::kCameraMaxPitchRadians,
        defs::kCameraMaxPitchRadians);
}

/**
 * @brief 正規化された視線方向を返す。
 */
general::vec::Vector3 Camera::Forward() const noexcept
{
    const auto cp = std::cos(m_fPitch);
    return {std::cos(m_fYaw) * cp, std::sin(m_fPitch), std::sin(m_fYaw) * cp};
}

/**
 * @brief 視線方向と右方向に移動する。
 * @param i_forward 前方移動量。
 * @param i_right 右方移動量。
 */
void Camera::Move(const float i_forward, const float i_right)
{
    if (!std::isfinite(i_forward) || !std::isfinite(i_right))
    {
        throw std::invalid_argument("カメラの移動量は有限値で指定してください。");
    }

    // 前方と右方向の移動を計算する
    const auto  forward =   Forward();
    auto        right   =   forward.Cross({0.0F, 1.0F, 0.0F});
    right.Normalize();
    const auto position =   m_instPosition + forward * i_forward + right * i_right;
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
    {
        throw std::overflow_error("カメラの位置が表現可能な範囲を超えました。");
    }
    m_instPosition  =   position;
}

/**
 * @brief 列優先のビュー行列を返す。
 */
std::array<float, 16> Camera::View() const
{
    const auto  forward =   Forward();
    auto        right   =   forward.Cross({0.0F, 1.0F, 0.0F});
    right.Normalize();

    // 上方向を計算する
    const auto up = right.Cross(forward);
    return {right.x, up.x, -forward.x, 0.0F,
            right.y, up.y, -forward.y, 0.0F,
            right.z, up.z, -forward.z, 0.0F,
            -right.Dot(m_instPosition), -up.Dot(m_instPosition),
            forward.Dot(m_instPosition), 1.0F};
}

/**
 * @brief 列優先の透視投影行列を返す。
 * @param i_aspect 横幅と高さの比率。
 * @return 透視投影行列。
 * @throws std::invalid_argument 比率が有限の正数でない場合。
 */
std::array<float, 16> Camera::Projection(const float i_aspect)
{
    if (!std::isfinite(i_aspect) || i_aspect <= 0.0F)
    {
        throw std::invalid_argument("画面の縦横比は有限の正数で指定してください。");
    }
    const auto nearPlane = defs::kCameraNearPlane;
    const auto farPlane = defs::kCameraFarPlane;
    const auto f = 1.0F / std::tan(defs::kCameraVerticalFovRadians / 2.0F);
    if (!std::isfinite(f / i_aspect))
    {
        throw std::invalid_argument("画面の縦横比が小さすぎます。");
    }
    return {f / i_aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (farPlane + nearPlane) / (nearPlane - farPlane), -1,
            0, 0, (2 * farPlane * nearPlane) / (nearPlane - farPlane), 0};
}

}   // namespace render
