/**
 * @file matrix4.cpp
 * @brief 変換行列の演算を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "matrix4.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace general::math
{
namespace
{

/**
 * @brief   double型の値をチェックし、float型に変換する。
 * @param   i_value チェック対象の値。
 * @return  float型に変換された値。
 * @throws  std::overflow_error 演算結果が表現可能な範囲外の場合。
 */
float CheckedFloat(const double i_value)
{
    if (!std::isfinite(i_value) || std::abs(i_value) > std::numeric_limits<float>::max())
    {
        throw std::overflow_error("行列の演算結果が表現可能な範囲を超えました。");
    }
    return static_cast<float>(i_value);
}
}

/**
 * @brief   回転行列を作成する。
 * @param   i_rotation 回転を表すクォータニオン。
 * @return  回転行列。
 */
Matrix4 Matrix4::Rotation(const Quaternion& i_rotation)
{
    return Transform(vec::Vector3(), i_rotation, vec::Vector3(1.0F, 1.0F, 1.0F));
}


/**
 * @brief   変換行列を作成する。
 * @param   i_position  位置。
 * @param   i_rotation  回転を表すクォータニオン。
 * @param   i_scale     スケール。
 * @return  変換行列。
 * @throws  std::invalid_argument 位置またはスケールが有限値でない場合。
 */
Matrix4 Matrix4::Transform(const vec::Vector3&  i_position,
                           const Quaternion&    i_rotation,
                           const vec::Vector3&  i_scale)
{
    // 位置とスケールが有限値であることをチェックする。
    if (!std::isfinite(i_position.x) || !std::isfinite(i_position.y) || !std::isfinite(i_position.z) ||
        !std::isfinite(i_scale.x) || !std::isfinite(i_scale.y) || !std::isfinite(i_scale.z))
    {
        throw std::invalid_argument("位置とスケールは有限値で指定してください。");
    }

    // クォータニオンで回転を表す値を計算
    const auto x = static_cast<double>(i_rotation.X()), y = static_cast<double>(i_rotation.Y());
    const auto z = static_cast<double>(i_rotation.Z()), w = static_cast<double>(i_rotation.W());
    auto result = Matrix4();
    auto& a = result.m_instValues;
    a[0] = CheckedFloat((1.0 - 2.0 * (y * y + z * z)) * i_scale.x);
    a[1] = CheckedFloat((2.0 * (x * y + w * z)) * i_scale.x);
    a[2] = CheckedFloat((2.0 * (x * z - w * y)) * i_scale.x);
    a[4] = CheckedFloat((2.0 * (x * y - w * z)) * i_scale.y);
    a[5] = CheckedFloat((1.0 - 2.0 * (x * x + z * z)) * i_scale.y);
    a[6] = CheckedFloat((2.0 * (y * z + w * x)) * i_scale.y);
    a[8] = CheckedFloat((2.0 * (x * z + w * y)) * i_scale.z);
    a[9] = CheckedFloat((2.0 * (y * z - w * x)) * i_scale.z);
    a[10] = CheckedFloat((1.0 - 2.0 * (x * x + y * y)) * i_scale.z);
    a[12] = i_position.x; a[13] = i_position.y; a[14] = i_position.z;
    return result;
}

vec::Vector3 Matrix4::TransformPoint(const vec::Vector3& i_point) const
{
    if (!std::isfinite(i_point.x) || !std::isfinite(i_point.y) || !std::isfinite(i_point.z))
    {
        throw std::invalid_argument("位置は有限値で指定してください。");
    }
    return vec::Vector3(
        CheckedFloat(static_cast<double>(m_instValues[0]) * i_point.x + static_cast<double>(m_instValues[4]) * i_point.y + static_cast<double>(m_instValues[8]) * i_point.z + m_instValues[12]),
        CheckedFloat(static_cast<double>(m_instValues[1]) * i_point.x + static_cast<double>(m_instValues[5]) * i_point.y + static_cast<double>(m_instValues[9]) * i_point.z + m_instValues[13]),
        CheckedFloat(static_cast<double>(m_instValues[2]) * i_point.x + static_cast<double>(m_instValues[6]) * i_point.y + static_cast<double>(m_instValues[10]) * i_point.z + m_instValues[14]));
}

/**
 * @brief   2つの行列の積を計算する。
 * @param   i_other 右辺の行列。
 * @return  積の行列。
 */
Matrix4 Matrix4::operator*(const Matrix4& i_other) const
{
    auto result = Matrix4();
    for (auto column = std::size_t{}; column < 4; ++column)
    {
        for (auto row = std::size_t{}; row < 4; ++row)
        {
            auto sum = 0.0;
            for (auto index = std::size_t{}; index < 4; ++index)
            {
                sum += static_cast<double>(m_instValues[index * 4 + row]) *
                    i_other.m_instValues[column * 4 + index];
            }
            result.m_instValues[column * 4 + row] = CheckedFloat(sum);
        }
    }
    return result;
}
}   // namespace general::math
