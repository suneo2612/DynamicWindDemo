/**
 * @file marching_cubes.cpp
 * @brief マーチングキューブの境界面と立方体の外面から占有体積を求める
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "marching_cubes.h"
#include "../defs/wind_region_constants.h"
#include "defs/marching_cube_constraints.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace demo::wind::simulation::detail
{
namespace
{
namespace windDefs = demo::wind::defs;

/**
 * @brief 3 次元座標を表す型。
 */
using Point = std::array<double, windDefs::kAxisCount>;

constexpr auto kSurfaceThreshold            =   0.0;                                    // 境界値の内外判定と面中心の曖昧さ判定に使うしきい値
constexpr auto kZeroVolume                  =   0.0;                                    // 体積の初期値、下限、および内側頂点がない場合の戻り値
constexpr auto kUnitCubeVolume              =   1.0;                                    // 単位立方体の体積と、体積の上限
constexpr auto kVolumeTolerance             =   1.0e-10;                                // 浮動小数点誤差を許容する体積範囲の判定幅
constexpr auto kNoEdgeIndex                 =   -1;                                     // next 配列で接続先が未登録であることを表す値
constexpr auto kMinimumPolygonVertexCount   =   std::size_t{3};                         // 面や輪郭を有効な多角形として扱う最小頂点数
constexpr auto kEdgeIntersectionOffset      =   static_cast<int>(defs::kVertexCount);   // 交点番号を立方体頂点番号と区別する開始位置

/**
 * @brief 頂点番号から各軸の座標ビットを取り出すマスク。
 */
constexpr auto kVertexCoordinateBitMask = std::size_t{1};

/**
 * @brief 2 頂点を結ぶ立方体の辺の番号を求める。
 */
int EdgeIndex(const int i_first, const int i_second)
{
    // 指定した2頂点を結ぶ辺の番号を探す
    for (auto index = std::size_t{}; index < defs::kEdges.size(); ++index)
    {
        const auto& edge = defs::kEdges[index];
        if ((edge.firstVertex == i_first && edge.secondVertex == i_second) ||
            (edge.firstVertex == i_second && edge.secondVertex == i_first))
        {
            return static_cast<int>(index);
        }
    }
    throw std::logic_error("立方体の辺が見つかりません。");
}

/**
 * @brief   外向きの三角形と原点で作る符号付き体積を求める。
 * @details b と c の外積を求め、その外積と a の内積（スカラー三重積）を計算する。
 * スカラー三重積は原点と三角形が作る四面体の符号付き体積の 6 倍になるため、
 * 6 で割ると三角形に対応する符号付き体積が得られる。符号は頂点の並び方向を表し、
 * 閉じた外向きの面を構成する三角形の値を合計すると、囲まれた領域の体積になる。
 */
double TriangleVolume(const Point& i_a, const Point& i_b, const Point& i_c)
{
    constexpr auto kTetrahedronVolumeDivisor = 6.0;
    // b と c の外積の X 成分。Y・Z 成分と合わせて面の法線方向を表す。
    const auto crossProductX =
        i_b[windDefs::kAxisYIndex] * i_c[windDefs::kAxisZIndex] -
        i_b[windDefs::kAxisZIndex] * i_c[windDefs::kAxisYIndex];
    // b と c の外積の Y 成分。
    const auto crossProductY =
        i_b[windDefs::kAxisZIndex] * i_c[windDefs::kAxisXIndex] -
        i_b[windDefs::kAxisXIndex] * i_c[windDefs::kAxisZIndex];
    // b と c の外積の Z 成分。
    const auto crossProductZ =
        i_b[windDefs::kAxisXIndex] * i_c[windDefs::kAxisYIndex] -
        i_b[windDefs::kAxisYIndex] * i_c[windDefs::kAxisXIndex];
    // a と外積の内積。頂点の順序によって符号が変わるスカラー三重積。
    const auto scalarTripleProduct =
        i_a[windDefs::kAxisXIndex] * crossProductX +
        i_a[windDefs::kAxisYIndex] * crossProductY +
        i_a[windDefs::kAxisZIndex] * crossProductZ;
    return scalarTripleProduct / kTetrahedronVolumeDivisor;
}

/**
 * @brief 面の4頂点と各辺の交点を格納する多角形の頂点容量。
 */
constexpr auto kFacePolygonVertexCapacity = defs::kVerticesPerFace * 2;

/**
 * @brief   外面に追加する多角形の頂点番号と有効数。
 * @details 頂点番号は vertices の先頭 count 個に巡回順で格納する。
 */
struct FacePolygon
{
    std::array<int, kFacePolygonVertexCapacity> vertices{}; // 多角形を構成する頂点番号
    std::size_t                                 count = 0;  // vertices 内で有効な頂点数
};

/**
 * @brief 一つの立方体の交点と閉じた面の体積を組み立てる。
 */
struct CubeSurface
{
    std::array<Point, defs::kVertexCount + defs::kEdgeCount> vertices{}; // 頂点と辺の交点
    std::array<int, defs::kEdgeCount> next{}; // 境界面の輪郭上で次に進む辺
    double volume = kZeroVolume; // 立方体の体積

    /**
     * @brief 外面の内側多角形による体積を加算し、境界面輪郭の接続先を記録する。
     * @param i_polygon 外側から見て反時計回りに並べた多角形。
     */
    void AddFace(const FacePolygon& i_polygon)
    {
        // 先頭頂点から扇状に三角形へ分割し、セル外面の符号付き体積を加算する。
        for (auto index = std::size_t{1}; index + 1 < i_polygon.count; ++index)
        {
            volume += TriangleVolume(vertices[i_polygon.vertices[0]],
                                     vertices[i_polygon.vertices[index]],
                                     vertices[i_polygon.vertices[index + 1]]);
        }
        
        // 交点同士を結ぶ輪郭区間を、セル外面と逆向きの接続として記録する。
        for (auto index = std::size_t{}; index < i_polygon.count; ++index)
        {
            const auto first    =   i_polygon.vertices[index];
            const auto second   =   i_polygon.vertices[(index + 1) % i_polygon.count];
            if (first < kEdgeIntersectionOffset || second < kEdgeIntersectionOffset) { continue; }

            const auto nextIndex = static_cast<std::size_t>(second - kEdgeIntersectionOffset);
            if (next[nextIndex] != kNoEdgeIndex) { throw std::logic_error("境界面の辺が重複しています。"); }
            next[nextIndex] = first - kEdgeIntersectionOffset;
        }
    }

    /**
     * @brief   記録された交点をつなぎ、境界面を三角形に分けて体積を追加する。
     * @throws  std::logic_error 交点のつながりが輪にならない場合。
     */
    void AddBoundary()
    {
        auto visited = std::array<bool, defs::kEdgeCount>{};
        
        // next 配列に記録された交点間の接続をたどり、未処理の輪郭を順に取り出す。
        for (auto start = std::size_t{}; start < defs::kEdgeCount; ++start)
        {
            // 接続のない辺と、すでに別の輪郭で処理した辺は対象外にする。
            if (next[start] == kNoEdgeIndex || visited[start]) { continue; }
            auto loop   =   std::array<int, defs::kEdgeCount>{};
            auto count  =   std::size_t{};
            auto edge   =   static_cast<int>(start);

            // 次の辺をたどって輪郭を保存し、開始位置に戻るまで閉じているか調べる。
            do
            {
                // 範囲外への移動、輪郭内の再訪、または辺数超過は不正な接続として扱う。
                if (edge < 0 || static_cast<std::size_t>(edge) >= defs::kEdgeCount ||
                    visited[static_cast<std::size_t>(edge)] || count >= defs::kEdgeCount)
                {
                    throw std::logic_error("境界面の輪郭を閉じることができません。");
                }
                const auto edgeIndex    =   static_cast<std::size_t>(edge);
                visited[edgeIndex]      =   true;
                loop[count++]           =   edge;
                edge                    =   next[edgeIndex];
            } while (edge != static_cast<int>(start));
            if (count < kMinimumPolygonVertexCount) 
            {
                throw std::logic_error("境界面の頂点が不足しています。");
            }

            // 閉じた輪郭を先頭頂点から扇状に分け、境界面の符号付き体積を加算する。
            for (auto index = std::size_t{1}; index + 1 < count; ++index)
            {
                volume += TriangleVolume(
                    vertices[static_cast<std::size_t>(kEdgeIntersectionOffset + loop[0])],
                    vertices[static_cast<std::size_t>(kEdgeIntersectionOffset + loop[index])],
                    vertices[static_cast<std::size_t>(kEdgeIntersectionOffset + loop[index + 1])]);
            }
        }
    }
};

}

/**
 * @brief   単位立方体内の境界面を構成し、内側領域の体積を求める。
 * @param   i_values        各頂点の境界値。0 以下を内側として扱う。
 * @param   i_faceValues    各面の中心の境界値。順序は X 最小、X 最大、Y 最小、Y 最大、Z 最小、Z 最大。
 * @return  閉じた近似領域の体積。範囲は 0 以上 1 以下。
 * @details 
 * 辺上の交点を線形補間で求め、面ごとに内側の多角形を作る。
 * 対角に内側の頂点がある面は、面中心の境界値に基づいて接続を決める。
 * @throws  std::logic_error 境界面を閉じられない場合、または計算体積が単位立方体の範囲外の場合。
 */
double MarchingCubeVolume(const std::array<double, defs::kVertexCount>& i_values,
                          const std::array<double, defs::kFaceCount>& i_faceValues)
{
    // 各頂点を境界値で内外に分け、後続処理で面が必要か判定する。
    auto inside = std::array<bool, defs::kVertexCount>{};
    auto count = std::size_t{};
    for (auto index = std::size_t{}; index < defs::kVertexCount; ++index)
    {
        inside[index] = i_values[index] <= kSurfaceThreshold;
        if (inside[index]) { ++count; }
    }
    // 全頂点が同じ側なら境界面が存在しないため、体積を直接返す。
    if (count == 0) { return kZeroVolume; }
    if (count == defs::kVertexCount) { return kUnitCubeVolume; }

    auto surface = CubeSurface{};
    // 未設定の交点接続を明示し、頂点番号の各ビットから単位立方体の座標を復元する。
    surface.next.fill(kNoEdgeIndex);
    for (auto index = std::size_t{}; index < defs::kVertexCount; ++index)
    {
        surface.vertices[index] = {
            static_cast<double>((index >> windDefs::kAxisXIndex) & kVertexCoordinateBitMask),
            static_cast<double>((index >> windDefs::kAxisYIndex) & kVertexCoordinateBitMask),
            static_cast<double>((index >> windDefs::kAxisZIndex) & kVertexCoordinateBitMask)};
    }

    // 内外が異なる辺だけに交点を作る。境界値 0 の頂点も同じ規則で扱う。
    for (auto index = std::size_t{}; index < defs::kEdgeCount; ++index)
    {
        const auto& edge = defs::kEdges[index];
        const auto a = edge.firstVertex;
        const auto b = edge.secondVertex;
        if (inside[a] == inside[b]) { continue; }
        // 境界値を線形補間し、辺上で境界値がゼロになる交点を求める。
        const auto ratio = i_values[a] / (i_values[a] - i_values[b]);
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            surface.vertices[static_cast<std::size_t>(kEdgeIntersectionOffset) + index][axis] =
                surface.vertices[a][axis] +
                ratio * (surface.vertices[b][axis] - surface.vertices[a][axis]);
        }
    }

    // 6 面それぞれで内側の多角形を作る。同時に境界面の輪郭も決まる。
    for (auto faceIndex = std::size_t{}; faceIndex < defs::kFaceCount; ++faceIndex)
    {
        const auto& face = defs::kFaces[faceIndex];
        const auto& [firstCorner, secondCorner, oppositeCorner, fourthCorner] = face;
        const auto ambiguous = inside[firstCorner] == inside[oppositeCorner] &&
                               inside[secondCorner] == inside[fourthCorner] &&
                               inside[firstCorner] != inside[secondCorner];
        if (ambiguous && i_faceValues[faceIndex] > kSurfaceThreshold)
        {
            // 対角の内側が離れている場合は、それぞれ独立した三角形にする。
            for (auto index = std::size_t{}; index < defs::kVerticesPerFace; ++index)
            {
                if (!inside[face[index]]) { continue; }
                const auto previous = face[(index + defs::kVerticesPerFace - 1) %
                                           defs::kVerticesPerFace];
                const auto current = face[index];
                const auto next = face[(index + 1) % defs::kVerticesPerFace];
                surface.AddFace({{{kEdgeIntersectionOffset + EdgeIndex(previous, current), current,
                                   kEdgeIntersectionOffset + EdgeIndex(current, next)}},
                                 kMinimumPolygonVertexCount});
            }
        }
        else
        {
            // 面の頂点順に内側の頂点と交点を並べ、面上の内側領域を一つの多角形にする。
            auto polygon = FacePolygon{};
            for (auto index = std::size_t{}; index < defs::kVerticesPerFace; ++index)
            {
                const auto current = face[index];
                const auto next = face[(index + 1) % defs::kVerticesPerFace];
                if (inside[current]) { polygon.vertices[polygon.count++] = current; }
                if (inside[current] != inside[next])
                {
                    polygon.vertices[polygon.count++] =
                        kEdgeIntersectionOffset + EdgeIndex(current, next);
                }
            }
            if (polygon.count >= kMinimumPolygonVertexCount) { surface.AddFace(polygon); }
        }
    }
    // 各面で記録した輪郭を閉じ、切断面の体積を加算する。
    surface.AddBoundary();
    // 丸め誤差の範囲を超えた異常値は拒否し、許容範囲内のわずかな超過は補正する。
    if (surface.volume < -kVolumeTolerance ||
        surface.volume > kUnitCubeVolume + kVolumeTolerance)
    {
        throw std::logic_error("境界面から求めた体積が立方体の範囲外です。");
    }
    return std::clamp(surface.volume, kZeroVolume, kUnitCubeVolume);
}

}   // namespace demo::wind::simulation
