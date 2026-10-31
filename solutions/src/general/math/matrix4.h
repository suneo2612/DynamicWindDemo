/**
 * @file matrix4.h
 * @brief 列ベクトルに作用する列優先 4 次元行列を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_GENERAL_MATH_MATRIX4_H
#define DYNAMIC_WIND_DEMO_GENERAL_MATH_MATRIX4_H

#include "quaternion.h"
#include <array>
#include <cstddef>

namespace general::math
{

/**
 * @brief 列優先で格納する 4 行 4 列の変換行列。
 */
class Matrix4 final
{
public:
    /** @brief 単位行列を作る。 */
    Matrix4() noexcept = default;

    /**
     * @brief 四元数の回転を列優先の 4×4 回転行列に変換する。
     * @param i_rotation 回転。
     * @return 平行移動とスケールを含まない回転行列。
     */
    static Matrix4 Rotation(const Quaternion& i_rotation);

    /**
     * @brief 移動、回転、スケールをこの順の行列積にする。
     * @param i_position 移動量。
     * @param i_rotation 回転。
     * @param i_scale スケール。ゼロおよび負値も許容する。
     * @return T * R * S。
     * @throws std::invalid_argument 位置またはスケールが非有限の場合。
     */
    static Matrix4 Transform(const vec::Vector3& i_position,
        const Quaternion& i_rotation, const vec::Vector3& i_scale);

    /** 
     * @brief 列優先の成分配列を返す。 
     */
    const std::array<float, 16>& Data() const noexcept { return m_instValues; }

    /**
     * @brief 列ベクトルの位置を変換する。
     * @param i_point 変換対象。
     * @return 変換後の位置。
     */
    vec::Vector3 TransformPoint(const vec::Vector3& i_point) const;

    /**
     * @brief 行列積を求める。
     * @param i_other 右側の行列。
     * @return 行列積。
     * @throws std::overflow_error 演算結果が表現可能な範囲外の場合。
     */
    Matrix4 operator*(const Matrix4& i_other) const;

private:
    /**
     * @brief   列優先の成分配列。
     */
    std::array<float, 16> m_instValues{
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
    };
};

}
#endif
