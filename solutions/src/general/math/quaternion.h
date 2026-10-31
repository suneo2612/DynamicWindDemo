/**
 * @file quaternion.h
 * @brief 右手系の回転を表す四元数を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_GENERAL_MATH_QUATERNION_H
#define DYNAMIC_WIND_DEMO_GENERAL_MATH_QUATERNION_H

#include "../vec/Vector3.h"

namespace general::math
{

/**
 * @brief 単位四元数による回転。合成は右側の回転を先に適用する。
 */
class Quaternion final
{
public:
    /** @brief 回転なしで初期化する。 */
    Quaternion() noexcept = default;

    /**
     * @brief 成分を正規化して回転を作る。
     * @param i_x X 成分。
     * @param i_y Y 成分。
     * @param i_z Z 成分。
     * @param i_w W 成分。
     * @throws std::invalid_argument 成分が非有限または長さがゼロの場合。
     */
    Quaternion(float i_x, float i_y, float i_z, float i_w);

    /**
     * @brief 固定軸 X、Y、Z の順に適用する角度から回転を作る。
     * @param i_angles ラジアン単位の各軸角度。
     * @return 対応する回転。
     * @throws std::invalid_argument 角度が非有限の場合。
     */
    static Quaternion FromEulerRadians(const vec::Vector3& i_angles);

    /**
     * @brief 固定軸 X、Y、Z の順に適用する角度から回転を作る。
     * @param i_angles 度単位の各軸角度。
     * @return 対応する回転。
     * @throws std::invalid_argument 角度が非有限の場合。
     */
    static Quaternion FromEulerDegrees(const vec::Vector3& i_angles);

    /**
     * @brief 固定軸 X、Y、Z の順のラジアン角度を取得する。
     * @return 同じ姿勢を表す角度。特異点では X を 0 とする。
     */
    vec::Vector3 ToEulerRadians() const;

    /** @brief 同じ姿勢を表す固定軸 X、Y、Z の度単位角度を返す。 */
    vec::Vector3 ToEulerDegrees() const;

    /** @brief 四元数の X 成分を返す。 */
    float X() const noexcept { return m_fX; }
    /** @brief 四元数の Y 成分を返す。 */
    float Y() const noexcept { return m_fY; }
    /** @brief 四元数の Z 成分を返す。 */
    float Z() const noexcept { return m_fZ; }
    /** @brief 四元数の W 成分を返す。 */
    float W() const noexcept { return m_fW; }

    /**
     * @brief ベクトルに回転を適用する。
     * @param i_vector 回転対象。
     * @return 回転後のベクトル。
     * @throws std::invalid_argument 成分が非有限の場合。
     * @throws std::overflow_error 演算結果が表現可能な範囲外の場合。
     */
    vec::Vector3 Rotate(const vec::Vector3& i_vector) const;

    /**
     * @brief 右側を先に適用する回転の合成。
     * @param i_other 先に適用する回転。
     * @return 合成された回転。
     */
    Quaternion operator*(const Quaternion& i_other) const;

private:
    float m_fX = 0.0F;
    float m_fY = 0.0F;
    float m_fZ = 0.0F;
    float m_fW = 1.0F;
};

}
#endif
