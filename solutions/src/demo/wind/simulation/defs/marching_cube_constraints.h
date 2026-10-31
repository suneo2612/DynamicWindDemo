/**
 * @file marching_cube_constraints.h
 * @brief マーチングキューブの立方体頂点に対する辺と面の対応を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_DEFS_MARCHING_CUBE_CONSTRAINTS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_DEFS_MARCHING_CUBE_CONSTRAINTS_H

#include <array>
#include <cstddef>

namespace demo::wind::simulation::detail::defs
{

// ================================================================
//  コンテナ定数のサイズなど
// ================================================================
/**
 * @brief 立方体を構成する頂点の数。
 */
inline constexpr auto kVertexCount      =   std::size_t{8};

/**
 * @brief 立方体を構成する辺の数。
 */
inline constexpr auto kEdgeCount        =   std::size_t{12};

/**
 * @brief 立方体を構成する面の数。
 */
inline constexpr auto kFaceCount        =   std::size_t{6};

/**
 * @brief 1面を表す頂点番号の数。
 */
inline constexpr auto kVerticesPerFace  =   std::size_t{4};


// ================================================================
//  列挙値定義
// ================================================================

/**
 * @brief 立方体の辺を位置関係で指定する名前。
 * @details X 最小側を左、X 最大側を右、Y 最小側を下、Y 最大側を上、
 * Z 最小側を前、Z 最大側を後ろとして表す。値の順序は kEdges の要素番号に対応する。
 */
enum class CubeEdge : std::size_t
{
    BottomFront,    // 下前側の X 方向の辺
    TopFront,       // 上前側の X 方向の辺
    BottomBack,     // 下後ろ側の X 方向の辺
    TopBack,        // 上後ろ側の X 方向の辺
    LeftFront,      // 左前側の Y 方向の辺
    RightFront,     // 右前側の Y 方向の辺
    LeftBack,       // 左後ろ側の Y 方向の辺
    RightBack,      // 右後ろ側の Y 方向の辺
    BottomLeft,     // 下左側の Z 方向の辺
    BottomRight,    // 下右側の Z 方向の辺
    TopLeft,        // 上左側の Z 方向の辺
    TopRight        // 上右側の Z 方向の辺
};

/**
 * @brief 立方体の面を位置関係で指定する名前。
 * @details X 最小側を左、X 最大側を右、Y 最小側を下、Y 最大側を上、
 * Z 最小側を前、Z 最大側を後ろとして表す。値の順序は kFaces の要素番号に対応する。
 */
enum class CubeFace : std::size_t
{
    Left,
    Right,
    Bottom,
    Top,
    Front,
    Back
};


// ================================================================
//  辺と面の対応
// ================================================================
/**
 * @brief 立方体の辺を構成する両端の頂点番号。
 */
struct EdgeVertices
{
    int firstVertex;    // 辺の始点頂点番号
    int secondVertex;   // 辺の終点頂点番号
};

/**
 * @brief 立方体の各辺を結ぶ頂点番号の組。
 * @details 頂点番号は X、Y、Z をそれぞれビット 0、1、2 として表す。
 */
inline constexpr auto kEdges = std::array<EdgeVertices, kEdgeCount>{{
    {0, 1},
    {2, 3},
    {4, 5},
    {6, 7},
    {0, 2},
    {1, 3},
    {4, 6},
    {5, 7},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7}
}};

/**
 * @brief 立方体の各面を外側から見て反時計回りに並べた頂点番号。
 */
inline constexpr auto kFaces = std::array<std::array<int, kVerticesPerFace>, kFaceCount>{{
    {0, 4, 6, 2},
    {1, 3, 7, 5},
    {0, 1, 5, 4},
    {2, 6, 7, 3},
    {0, 2, 3, 1},
    {4, 5, 7, 6}
}};

/**
 * @brief 列挙名で指定された立方体の辺を結ぶ頂点番号を取得する。
 * @param i_edge 取得する辺の位置関係。
 * @return 辺の両端を表す頂点番号への参照。
 */
constexpr const EdgeVertices& GetEdgeVertices(const CubeEdge i_edge) noexcept
{
    return kEdges[static_cast<std::size_t>(i_edge)];
}

/**
 * @brief 列挙名で指定された立方体の面を構成する頂点番号を取得する。
 * @param i_face 取得する面の位置関係。
 * @return 面を構成する頂点番号の配列への参照。
 */
constexpr const std::array<int, kVerticesPerFace>& GetFaceVertices(const CubeFace i_face) noexcept
{
    return kFaces[static_cast<std::size_t>(i_face)];
}

}
#endif