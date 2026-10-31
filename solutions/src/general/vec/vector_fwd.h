/**
 * @file vector_fwd.h
 * @brief ベクトルの前方宣言と型別名を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_FWD_H
#define DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_FWD_H

#include <cstddef>

namespace general::vec
{

template <typename T, std::size_t N>
class Vector;

// float 型のベクトル型の別名
using Vector2 = Vector<float, 2>;
using Vector3 = Vector<float, 3>;
using Vector4 = Vector<float, 4>;

// double 型のベクトル型の別名
using DoubleVector2 = Vector<double, 2>;
using DoubleVector3 = Vector<double, 3>;
using DoubleVector4 = Vector<double, 4>;

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_VEC_VECTOR_FWD_H
