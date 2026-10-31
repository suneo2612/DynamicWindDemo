/**
 * @file Vector4.h
 * @brief 4次元ベクトルのテンプレート特殊化を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR4_H
#define DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR4_H

#include "vector_base.h"

namespace general::vec
{

/**
 * @brief 4次元ベクトルを表すテンプレート特殊化。
 * @tparam T 成分型。float または double。
 */
template <typename T>
class Vector<T, 4> final : public detail::VectorOperations<T, 4, Vector<T, 4>>
{
public:
    T x{};
    T y{};
    T z{};
    T w{};

    /**
     * @brief 全成分を0で初期化する。
     */
    Vector() noexcept = default;

    /**
     * @brief 4次元ベクトルを成分値で初期化する。
     * @param i_x X 成分。
     * @param i_y Y 成分。
     * @param i_z Z 成分。
     * @param i_w W 成分。
     */
    Vector(const T i_x, const T i_y, const T i_z, const T i_w) noexcept
        : x(i_x)
        , y(i_y)
        , z(i_z)
        , w(i_w)
    {
    }

    /**
     * @brief ベクトルの成分を添字で参照する。
     * @param i_index 0から始まる成分の添字。
     * @return 指定した成分への参照。
     * @throws std::out_of_range 添字が次元数以上の場合。
     */
    T& operator[](const std::size_t i_index)
    {
        switch (i_index)
        {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return w;
        default:
            throw std::out_of_range("ベクトルの成分添字が範囲外です。");
        }
    }

    /**
     * @brief ベクトルの成分を添字で参照する。
     * @param i_index 0から始まる成分の添字。
     * @return 指定した成分への定数参照。
     * @throws std::out_of_range 添字が次元数以上の場合。
     */
    const T& operator[](const std::size_t i_index) const
    {
        switch (i_index)
        {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return w;
        default:
            throw std::out_of_range("ベクトルの成分添字が範囲外です。");
        }
    }
};

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR4_H
