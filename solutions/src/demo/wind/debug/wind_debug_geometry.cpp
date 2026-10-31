/**
 * @file wind_debug_geometry.cpp
 * @brief 風の表示用線分を生成する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "wind_debug_geometry.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace demo::wind::debug
{
namespace
{
/**
 * @brief ワールド座標の XYZ 成分を倍精度で保持する。
 */
using Point = std::array<double, defs::kAxisCount>;

/**
 * @brief ワイヤーフレーム生成で扱う形状か判定する。
 * @param i_shape 判定する発生器形状。
 * @return 球、円錐、円柱のいずれかなら true。
 */
bool IsSupportedShape(const motor::Shape i_shape)
{
    switch (i_shape)
    {
    case motor::Shape::Sphere:
    case motor::Shape::Cone:
    case motor::Shape::Cylinder:
        return true;
    default:
        return false;
    }
}

/**
 * @brief ワイヤーフレーム生成に必要な発生器設定を検証する。
 * @param i_settings 検証する発生器設定。
 * @throws std::invalid_argument 位置、方向、形状または寸法が不正な場合。
 */
void ValidateWireframeSettings(const motor::Settings& i_settings)
{
    const auto finite = [](const float i_value) { return std::isfinite(i_value); };

    // 位置と方向が有限かどうかを確認する。
    const auto hasFinitePosition =  std::all_of(i_settings.position.begin(),
                                                i_settings.position.end(),
                                                finite);
    const auto hasFiniteDirection = std::all_of(i_settings.direction.begin(),
                                                i_settings.direction.end(),
                                                 finite);
    if (!hasFinitePosition || !hasFiniteDirection)
    {
        throw std::invalid_argument("表示する発生器の形状、位置または寸法が不正です。");
    }

    // 形状がサポートされているかどうかを確認する。
    if (!IsSupportedShape(i_settings.shape))
    {
        throw std::invalid_argument("表示する発生器の形状、位置または寸法が不正です。");
    }

    // 寸法が有効かどうかを確認する。
    const auto hasValidDiameter    = finite(i_settings.diameter) && i_settings.diameter > 0.0F;             // 直径が有限かつ正
    const auto hasFiniteLength     = finite(i_settings.length);                                             // 長さが有限
    const auto hasValidShapeLength = i_settings.shape == motor::Shape::Sphere || i_settings.length > 0.0F;  // 球以外の長さが正

    // 寸法または形状のいずれかが不正なら、描画用の設定として拒否する。
    if (!hasValidDiameter || !hasFiniteLength || !IsSupportedShape(i_settings.shape) || !hasValidShapeLength)
    {
        throw std::invalid_argument("表示する発生器の形状、位置または寸法が不正です。");
    }
}

/**
 * @brief 三次元ベクトルの外積を求める。
 * @param i_left 左側のベクトル。
 * @param i_right 右側のベクトル。
 * @return i_left と i_right の外積。
 */
Point Cross(const Point& i_left, const Point& i_right)
{
    return {i_left[defs::kAxisYIndex] * i_right[defs::kAxisZIndex] - i_left[defs::kAxisZIndex] * i_right[defs::kAxisYIndex],
            i_left[defs::kAxisZIndex] * i_right[defs::kAxisXIndex] - i_left[defs::kAxisXIndex] * i_right[defs::kAxisZIndex],
            i_left[defs::kAxisXIndex] * i_right[defs::kAxisYIndex] - i_left[defs::kAxisYIndex] * i_right[defs::kAxisXIndex]};
}

/**
 * @brief 倍精度の座標を検証し、描画用の単精度座標に変換する。
 * @param i_point 変換する XYZ 座標。
 * @return 描画用の XYZ 頂点。
 * @throws std::invalid_argument 座標が有限でない、または float で表現できない場合。
 */
std::array<float, defs::kAxisCount> ToVertex(const Point& i_point)
{
    auto result = std::array<float, defs::kAxisCount>{};
    for (auto axis = std::size_t{}; axis < result.size(); ++axis)
    {
        if (!std::isfinite(i_point[axis]) ||
            std::abs(i_point[axis]) > static_cast<double>(std::numeric_limits<float>::max()))
        {
            throw std::invalid_argument("風の表示を描画できる座標範囲を超えています。");
        }
        result[axis] = static_cast<float>(i_point[axis]);
    }
    return result;
}

}

/**
 * @brief 発生器の形状を、描画用の線分頂点列に変換する。
 * @param i_settings 形状、寸法、位置と方向を含む発生器設定。
 * @return 線分の始点と終点を交互に並べた XYZ 頂点列。
 * @throws std::invalid_argument 形状、寸法、位置、方向が不正、または頂点座標を float で表現できない場合。
 * @details 球は中心を通る円、円錐と円柱は底面と側面を表す線分で描く。
 * 曲線は 32 分割して近似する。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildWireframe(const motor::Settings& i_settings)
{
    ValidateWireframeSettings(i_settings);

    // 風のベクトルの長さを計算
    const auto directionLength = std::hypot(static_cast<double>(i_settings.direction[defs::kAxisXIndex]),
                                             static_cast<double>(i_settings.direction[defs::kAxisYIndex]),
                                             static_cast<double>(i_settings.direction[defs::kAxisZIndex]));
    if (directionLength == 0.0) { throw std::invalid_argument("表示する発生器の方向がゼロです。"); }

    // 軸に垂直な二方向を作り、傾いた円錐や円柱でも正しい円を描けるようにする。
    const auto axis = Point{i_settings.direction[defs::kAxisXIndex] / directionLength,
                            i_settings.direction[defs::kAxisYIndex] / directionLength,
                            i_settings.direction[defs::kAxisZIndex] / directionLength};

    // 軸に垂直な参照方向を選びつつ、側面方向ベクトルを計算する
    const auto reference    =   std::abs(axis[defs::kAxisYIndex]) < 0.9 ? Point{0.0, 1.0, 0.0} : Point{1.0, 0.0, 0.0};
    auto side               =   Cross(axis, reference);
    const auto sideLength   =   std::hypot(side[defs::kAxisXIndex], side[defs::kAxisYIndex], side[defs::kAxisZIndex]);
    for (auto& value : side) { value /= sideLength; }

    // 軸に垂直なもう一つの方向を計算する。
    const auto other    =   Cross(axis, side);
    const auto origin   =   Point{i_settings.position[defs::kAxisXIndex],
                                  i_settings.position[defs::kAxisYIndex],
                                  i_settings.position[defs::kAxisZIndex]};

    // 半径と風発生器長さを倍精度で計算する
    const auto radius = static_cast<double>(i_settings.diameter) * 0.5;
    const auto length = static_cast<double>(i_settings.length);

    constexpr auto segments =   32;                         // 円周を分割するセグメント数
    constexpr auto pi       =   3.14159265358979323846;     // 円周率
    auto lines              =   std::vector<std::array<float, defs::kAxisCount>>{};

    // 円周上の角度と軸方向の位置から、ワールド座標の一点を求める。
    const auto point = [&](const Point& i_first,
                           const Point& i_second,
                           const double i_angle,
                           const double i_offset)
    {
        auto result = origin;
        for (auto component = std::size_t{}; component < result.size(); ++component)
        {
            result[component] += axis[component] * i_offset +
                                 radius * (i_first[component] * std::cos(i_angle) +
                                 i_second[component] * std::sin(i_angle));
        }
        return result;
    };

    // 計算した二点を float 頂点に変換し、線分として頂点列へ追加する。
    const auto line = [&](const Point& i_first, const Point& i_second)
    {
        lines.push_back(ToVertex(i_first));
        lines.push_back(ToVertex(i_second));
    };

    // 円周を等間隔に分割し、隣り合う点を結んで円形の輪郭を作る。
    const auto ring = [&](const Point& i_first, const Point& i_second, const double i_offset)
    {
        for (auto segment = 0; segment < segments; ++segment)
        {
            const auto first    = 2.0 * pi * segment / segments;
            const auto second   = 2.0 * pi * (segment + 1) / segments;
            line(point(i_first, i_second, first, i_offset), point(i_first, i_second, second, i_offset));
        }
    };

    // 球は中心を通る三つの円、円錐と円柱は底面と側面の線で範囲を示す。
    if (i_settings.shape == motor::Shape::Sphere)
    {
        ring(side, other, 0.0);
        ring(axis, side, 0.0);
        ring(axis, other, 0.0);
    }
    else
    {
        ring(side, other, length);
        if (i_settings.shape == motor::Shape::Cylinder) { ring(side, other, 0.0); }
        for (auto segment = 0; segment < segments; segment += 4)
        {
            const auto angle = 2.0 * pi * segment / segments;
            const auto start = i_settings.shape == motor::Shape::Cone
                ? origin : point(side, other, angle, 0.0);
            line(start, point(side, other, angle, length));
        }
    }
    return lines;
}

/**
 * @brief 管理空間の境界を構成する直方体を線分に変換する。
 * @param i_origin 管理空間の最小 XYZ 座標。
 * @param i_size XYZ 各軸の正のセル数。各セルの辺は 1 m。
 * @return 直方体の 12 辺を表す始点・終点の XYZ 頂点列。
 * @throws std::invalid_argument セル数が正でない、または頂点座標を float で表現できない場合。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildRegionWireframe(
    const std::array<int, defs::kAxisCount>& i_origin,
    const std::array<int, defs::kAxisCount>& i_size)
{
    if (!std::all_of(i_size.begin(), i_size.end(), [](const int i_value) { return i_value > 0; }))
    {
        throw std::invalid_argument("管理空間のセル数は正の値で指定してください。");
    }
    auto corners = std::array<Point, 8>{};
    for (auto corner = std::size_t{}; corner < corners.size(); ++corner)
    {
        for (auto axis = std::size_t{}; axis < i_origin.size(); ++axis)
        {
            corners[corner][axis] = static_cast<double>(i_origin[axis]) +
                ((corner & (std::size_t{1} << axis)) != 0 ? i_size[axis] : 0);
        }
    }
    auto lines = std::vector<std::array<float, defs::kAxisCount>>{};
    lines.reserve(24);
    for (auto corner = std::size_t{}; corner < corners.size(); ++corner)
    {
        for (auto axis = std::size_t{}; axis < i_origin.size(); ++axis)
        {
            const auto bit = std::size_t{1} << axis;
            if ((corner & bit) == 0)
            {
                lines.push_back(ToVertex(corners[corner]));
                lines.push_back(ToVertex(corners[corner | bit]));
            }
        }
    }
    return lines;
}

/**
 * @brief 管理空間内の全セルの辺を、重複しない格子線に変換する。
 * @param i_origin 管理空間の最小 XYZ 座標（m）。
 * @param i_size XYZ 各軸の正のセル数。各セルの辺は 1 m。
 * @return 管理空間を格子状に分割する線分の頂点列。各線分は始点と終点の 2 頂点。
 * @throws std::invalid_argument セル数が正でない、または頂点座標を float で表現できない場合。
 * @throws std::overflow_error 1 m 単位の座標を表現できない、または線分数が OpenGL の上限を超える場合。
 * @details 各軸に平行な線分を一度だけ生成する。線分数を事前に検査し、描画可能な範囲を超える場合は拒否する。
 */
std::vector<std::array<float, defs::kAxisCount>> BuildCellGridWireframe(
    const std::array<int, defs::kAxisCount>& i_origin,
    const std::array<int, defs::kAxisCount>& i_size)
{
    // 空の範囲や逆向きの範囲を作らないよう、各軸に少なくとも 1 セル必要とする。
    if (!std::all_of(i_size.begin(),
                     i_size.end(),
                     [](const int i_value) 
                     {
                        return i_value > 0;
                     })
        )
    {
        throw std::invalid_argument("管理空間のセル数は正の値で指定してください。");
    }

    // float 座標でも隣り合う 1 m の境界を区別できるよう、原点と終端の精度を確認する。
    constexpr auto maxExactCoordinate   =   16777216.0;
    for (auto axis = std::size_t{}; axis < i_size.size(); ++axis)
    {
        const auto first    =   static_cast<double>(i_origin[axis]);
        const auto last     =   first + static_cast<double>(i_size[axis]);
        if (first < -maxExactCoordinate || last > maxExactCoordinate)
        {
            throw std::overflow_error("セルの境界座標を 1 m 単位で表現できません。");
        }
    }

    // 線分 1 本につき 2 頂点必要なため、描画可能な頂点数から線分数の上限を求める。
    const auto maxLines =   static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max()) / 2;
    auto lineCount      =   std::uint64_t{};
    for (auto axis = std::size_t{}; axis < i_size.size(); ++axis)
    {
        // この軸に平行な線の本数は、直交する二軸の格子点数の積になる。
        const auto first    =   static_cast<std::uint64_t>(i_size[(axis + 1) % defs::kAxisCount]) + 1;
        const auto second   =   static_cast<std::uint64_t>(i_size[(axis + 2) % defs::kAxisCount]) + 1;

        // 積を計算する前に上限を検査し、線分数の整数オーバーフローを防ぐ。
        if (first > (maxLines - lineCount) / second)
        {
            throw std::overflow_error("セル格子の頂点数が描画可能な範囲を超えています。");
        }
        lineCount += first * second;
    }
    auto lines  =   std::vector<std::array<float, defs::kAxisCount>>{};
    lines.reserve(static_cast<std::size_t>(lineCount * 2));

    // 各軸に平行な線を格子面上の全ての交点から伸ばし、各セル辺を一度だけ追加する。
    for (auto axis = std::size_t{}; axis < i_size.size(); ++axis)
    {
        // 線の始点を並べる二軸を決め、その格子点をすべて走査する。
        const auto firstAxis    =   (axis + 1) % defs::kAxisCount;
        const auto secondAxis   =   (axis + 2) % defs::kAxisCount;
        for (auto first = std::int64_t{}; first <= i_size[firstAxis]; ++first)
        {
            for (auto second = std::int64_t{}; second <= i_size[secondAxis]; ++second)
            {
                // 二つの直交軸上の位置を決め、対象軸の終端まで伸びる線分を作る。
                auto start  =   Point{static_cast<double>(i_origin[defs::kAxisXIndex]),
                                      static_cast<double>(i_origin[defs::kAxisYIndex]),
                                      static_cast<double>(i_origin[defs::kAxisZIndex])};

                // 二つの直交軸上の位置を決める
                start[firstAxis] += static_cast<double>(first);
                start[secondAxis] += static_cast<double>(second);

                // 対象軸に沿った線分の終点を作る
                auto end    =   start;
                end[axis]   +=  static_cast<double>(i_size[axis]);
                lines.push_back(ToVertex(start));
                lines.push_back(ToVertex(end));
            }
        }
    }
    return lines;
}

}
