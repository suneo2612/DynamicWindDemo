/**
 * @file vector_base.h
 * @brief 2～4次元ベクトルで共通する演算と型宣言を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_BASE_H
#define DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_BASE_H

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

#include "vector_fwd.h"

namespace general::vec
{

namespace detail
{

/**
 * @brief Vector の各次元で共通する演算を提供する。
 * @tparam T ベクトルの成分型。
 * @tparam N ベクトルの次元数。
 * @tparam Derived 演算結果として使用する Vector 特殊化。
 */
template <typename T, std::size_t N, typename Derived>
class VectorOperations
{
    static_assert(
        std::is_same_v<T, float> || std::is_same_v<T, double>,
        "ベクトルのスカラー型には float または double を指定してください。");

public:
    /**
     * @brief 2つのベクトルの内積を計算する。
     * @param i_other 内積の相手となるベクトル。
     * @return 2つのベクトルの内積。
     */
    T Dot(const Derived& i_other) const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        T result{};
        for (std::size_t index = 0; index < N; ++index)
        {
            result += self[index] * i_other[index];
        }
        return result;
    }

    /**
     * @brief ベクトルの長さの二乗を計算する。
     * @return ベクトルの長さの二乗。
     */
    T LengthSquared() const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        return Dot(self);
    }

    /**
     * @brief ベクトルの長さを計算する。
     * @return ベクトルの長さ。
     */
    T Length() const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        T length{};
        for (std::size_t index = 0; index < N; ++index)
        {
            length = std::hypot(length, self[index]);
        }
        return length;
    }

    /**
     * @brief ベクトル自身を正規化する。
     * @return 正規化前のベクトルの長さ。
     * @throws std::domain_error ベクトルの長さが0の場合。
     */
    T Normalize()
    {
        Derived& self = static_cast<Derived&>(*this);
        const T length = Length();
        if (length == T{})
        {
            throw std::domain_error("ゼロベクトルは正規化できません。");
        }

        for (std::size_t index = 0; index < N; ++index)
        {
            self[index] /= length;
        }
        return length;
    }

    /**
     * @brief 2つのベクトルを加算する。
     * @param i_other 加算するベクトル。
     * @return 加算結果。
     */
    Derived operator+(const Derived& i_other) const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        Derived result{};
        for (std::size_t index = 0; index < N; ++index)
        {
            result[index] = self[index] + i_other[index];
        }
        return result;
    }

    /**
     * @brief 2つのベクトルを減算する。
     * @param i_other 減算するベクトル。
     * @return 減算結果。
     */
    Derived operator-(const Derived& i_other) const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        Derived result{};
        for (std::size_t index = 0; index < N; ++index)
        {
            result[index] = self[index] - i_other[index];
        }
        return result;
    }

    /**
     * @brief ベクトルにスカラーを乗算する。
     * @param i_scalar 乗算するスカラー。
     * @return 乗算結果。
     */
    Derived operator*(const T i_scalar) const
    {
        const Derived& self = static_cast<const Derived&>(*this);
        Derived result{};
        for (std::size_t index = 0; index < N; ++index)
        {
            result[index] = self[index] * i_scalar;
        }
        return result;
    }

    /**
     * @brief スカラーにベクトルを乗算する。
     * @param i_scalar 乗算するスカラー。
     * @param i_vector 乗算するベクトル。
     * @return 乗算結果。
     */
    friend Derived operator*(const T i_scalar, const Derived& i_vector)
    {
        return i_vector * i_scalar;
    }
};

}

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_BASE_H
