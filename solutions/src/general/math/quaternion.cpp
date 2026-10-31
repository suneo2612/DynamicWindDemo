/**
 * @file quaternion.cpp
 * @brief 四元数とオイラー角の変換を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "quaternion.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace general::math
{
namespace
{

/**
 *  @brief ベクトルの成分が有限値であることを確認する。
 *  @param i_value 確認するベクトル。
 *  @throws std::invalid_argument 成分が有限値でない場合。
 */
void CheckFinite(const vec::Vector3& i_value)
{
    if (!std::isfinite(i_value.x) || !std::isfinite(i_value.y) || !std::isfinite(i_value.z))
    {
        throw std::invalid_argument("ベクトルの成分は有限値で指定してください。");
    }
}

}

/**
 * @brief 四元数を構築する。
 * @param i_x x 成分。
 * @param i_y y 成分。
 * @param i_z z 成分。
 * @param i_w w 成分。
 * @throws std::invalid_argument 成分が有限値でない場合、またはゼロ四元数の場合。
 */
Quaternion::Quaternion(const float i_x, const float i_y, const float i_z, const float i_w)
{
    // 成分が有限値であることを確認する。
    if (!std::isfinite(i_x) || !std::isfinite(i_y) ||
        !std::isfinite(i_z) || !std::isfinite(i_w))
    {
        throw std::invalid_argument("四元数の成分は有限値で指定してください。");
    }

    // ゼロ四元数でないことを確認する。
    const auto length = std::hypot(std::hypot(static_cast<double>(i_x), i_y),
                                   std::hypot(static_cast<double>(i_z), i_w));
    if (length == 0.0)
    {
        throw std::invalid_argument("ゼロ四元数は回転に使用できません。");
    }
    m_fX = static_cast<float>(i_x / length);
    m_fY = static_cast<float>(i_y / length);
    m_fZ = static_cast<float>(i_z / length);
    m_fW = static_cast<float>(i_w / length);
}

/**
 * @brief オイラー角から姿勢四元数を構築する。
 * @param i_angles オイラー角（ラジアン）。
 * @throws std::invalid_argument 成分が有限値でない場合。
 */
Quaternion Quaternion::FromEulerRadians(const vec::Vector3& i_angles)
{
    CheckFinite(i_angles);

    // オイラー角を半角に変換して回転
    const auto hx = static_cast<double>(i_angles.x) * 0.5;
    const auto hy = static_cast<double>(i_angles.y) * 0.5;
    const auto hz = static_cast<double>(i_angles.z) * 0.5;
    const auto cx = std::cos(hx), sx = std::sin(hx);
    const auto cy = std::cos(hy), sy = std::sin(hy);
    const auto cz = std::cos(hz), sz = std::sin(hz);
    return Quaternion(static_cast<float>(sx * cy * cz - cx * sy * sz),
                      static_cast<float>(cx * sy * cz + sx * cy * sz),
                      static_cast<float>(cx * cy * sz - sx * sy * cz),
                      static_cast<float>(cx * cy * cz + sx * sy * sz));
}

/**
 * @brief Degree角（度）から姿勢四元数を構築する。
 * @param i_angles Degree角（度）。
 * @throws std::invalid_argument 成分が有限値でない場合。
 */
Quaternion Quaternion::FromEulerDegrees(const vec::Vector3& i_angles)
{
    CheckFinite(i_angles);

    // Degree角をラジアンに変換する。
    constexpr auto factor = std::numbers::pi_v<double> / 180.0;
    return FromEulerRadians(vec::Vector3(static_cast<float>(i_angles.x * factor),
        static_cast<float>(i_angles.y * factor), static_cast<float>(i_angles.z * factor)));
}

/**
 * @brief 固定軸 X、Y、Z の順のラジアン角度を取得する。
 * @return 同じ姿勢を表す角度。特異点では X を 0 とする。
 */
vec::Vector3 Quaternion::ToEulerRadians() const
{
    const auto norm = std::hypot(std::hypot(static_cast<double>(m_fX), m_fY),
                                 std::hypot(static_cast<double>(m_fZ), m_fW));
    const auto x = m_fX / norm, y = m_fY / norm;
    const auto z = m_fZ / norm, w = m_fW / norm;
    const auto sinY = std::clamp(2.0 * (w * y - z * x), -1.0, 1.0);
    const auto cosY = std::hypot(1.0 - 2.0 * (y * y + z * z), 2.0 * (w * z + x * y));
    const auto angleY = std::atan2(sinY, cosY);
    if (cosY < 1.0e-7)
    {
        const auto m01 = 2.0 * (x * y - w * z);
        const auto m11 = 1.0 - 2.0 * (x * x + z * z);
        return vec::Vector3(0.0F, static_cast<float>(angleY),
                            static_cast<float>(std::atan2(-m01, m11)));
    }
    return vec::Vector3(
        static_cast<float>(std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y))),
        static_cast<float>(angleY),
        static_cast<float>(std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))));
}

/**
 * @brief 固定軸 X、Y、Z の順の Degree角（度）を取得する。
 * @return 同じ姿勢を表す角度。特異点では X を 0 とする。
 */
vec::Vector3 Quaternion::ToEulerDegrees() const
{
    const auto angles       =   ToEulerRadians();
    constexpr auto factor   =   180.0F / std::numbers::pi_v<float>;
    return angles * factor;
}

/**
 * @brief ベクトルに回転を適用する。
 * @param i_vector 回転対象。
 * @return 回転後のベクトル。
 * @throws std::invalid_argument 成分が非有限の場合。
 * @throws std::overflow_error 演算結果が表現可能な範囲外の場合。
 */
vec::Vector3 Quaternion::Rotate(const vec::Vector3& i_vector) const
{
    CheckFinite(i_vector);
    const auto x = static_cast<double>(m_fX), y = static_cast<double>(m_fY);
    const auto z = static_cast<double>(m_fZ), w = static_cast<double>(m_fW);
    const auto tx = 2.0 * (y * i_vector.z - z * i_vector.y);
    const auto ty = 2.0 * (z * i_vector.x - x * i_vector.z);
    const auto tz = 2.0 * (x * i_vector.y - y * i_vector.x);
    const auto rx = i_vector.x + w * tx + y * tz - z * ty;
    const auto ry = i_vector.y + w * ty + z * tx - x * tz;
    const auto rz = i_vector.z + w * tz + x * ty - y * tx;
    constexpr auto max = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(rx) || !std::isfinite(ry) || !std::isfinite(rz) ||
        std::abs(rx) > max || std::abs(ry) > max || std::abs(rz) > max)
    {
        throw std::overflow_error("回転結果が表現可能な範囲を超えました。");
    }
    return vec::Vector3(static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz));
}


/**
 * @brief 2つのクォータニオンの積を計算する。
 * @param i_other 右辺のクォータニオン。
 * @return 積のクォータニオン。
 */
Quaternion Quaternion::operator*(const Quaternion& i_other) const
{
    return Quaternion(
        m_fW * i_other.m_fX + m_fX * i_other.m_fW + m_fY * i_other.m_fZ - m_fZ * i_other.m_fY,
        m_fW * i_other.m_fY - m_fX * i_other.m_fZ + m_fY * i_other.m_fW + m_fZ * i_other.m_fX,
        m_fW * i_other.m_fZ + m_fX * i_other.m_fY - m_fY * i_other.m_fX + m_fZ * i_other.m_fW,
        m_fW * i_other.m_fW - m_fX * i_other.m_fX - m_fY * i_other.m_fY - m_fZ * i_other.m_fZ);
}
}
