/**
 * @file motor_rotation.cpp
 * @brief デバッグ発生器の固定軸回転を計算する。
 * @date 2026-11-01
 * @par 最終更新日 2026-11-01
 */
#include "motor_rotation.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace demo::wind::debug
{
void ValidateMotorRotation(const MotorRotation& i_rotation)
{
    if (!std::all_of(i_rotation.axis.begin(), i_rotation.axis.end(),
                     [](const auto i_value) { return std::isfinite(i_value); }) ||
        !std::isfinite(i_rotation.degreesPerSecond) ||
        std::hypot(static_cast<double>(i_rotation.axis[0]), i_rotation.axis[1], i_rotation.axis[2]) == 0.0)
    {
        throw std::invalid_argument("回転軸はゼロでない有限値、角速度は有限値で指定してください。");
    }
}

std::array<float, 3> RotateMotorDirection(const std::array<float, 3>& i_direction,
                                          const MotorRotation& i_rotation,
                                          const double i_elapsed)
{
    ValidateMotorRotation(i_rotation);
    const auto length = std::hypot(static_cast<double>(i_direction[0]), i_direction[1], i_direction[2]);
    if (!std::isfinite(length) || length == 0.0 || !std::isfinite(i_elapsed) || i_elapsed < 0.0)
    {
        throw std::invalid_argument("回転方向と経過時間が不正です。");
    }
    if (!i_rotation.enabled || i_rotation.degreesPerSecond == 0.0F || i_elapsed == 0.0)
    {
        return i_direction;
    }

    // 経過時間が非常に大きくても積がオーバーフローしないよう、先に一周分へ収める。
    constexpr auto fullTurn = 360.0;
    const auto speed = static_cast<double>(i_rotation.degreesPerSecond);
    const auto angle = std::fmod(i_elapsed, fullTurn / std::abs(speed)) * speed *
                       (std::numbers::pi_v<double> / 180.0);
    const auto axisLength = std::hypot(static_cast<double>(i_rotation.axis[0]),
                                       i_rotation.axis[1], i_rotation.axis[2]);
    const auto x = i_rotation.axis[0] / axisLength;
    const auto y = i_rotation.axis[1] / axisLength;
    const auto z = i_rotation.axis[2] / axisLength;
    const auto vx = i_direction[0] / length;
    const auto vy = i_direction[1] / length;
    const auto vz = i_direction[2] / length;
    const auto cosine = std::cos(angle);
    const auto sine = std::sin(angle);
    const auto projection = (x * vx + y * vy + z * vz) * (1.0 - cosine);
    return {static_cast<float>(vx * cosine + (y * vz - z * vy) * sine + x * projection),
            static_cast<float>(vy * cosine + (z * vx - x * vz) * sine + y * projection),
            static_cast<float>(vz * cosine + (x * vy - y * vx) * sine + z * projection)};
}
}
