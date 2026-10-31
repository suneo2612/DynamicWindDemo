/**
 * @file occupancy.cpp
 * @brief 風発生器とセルが重なる体積を近似する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "occupancy.h"
#include "occupancy_region.h"
#include "marching_cubes.h"
#include "../defs/wind_region_constants.h"
#include "../motor/defs/wind_motor_constraints.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace demo::wind::simulation
{
namespace
{
namespace axes = demo::wind::defs;
namespace cube = detail::defs;

constexpr auto kSurfaceThreshold    =   0.0;                                    // 形状の内側・境界・外側を境界関数の符号で分けるしきい値
constexpr auto kEmptyVolume         =   0.0;                                    // 重なりがない場合の体積と、占有体積の下限
constexpr auto kZeroDirectionLength =   0.0;                                    // 方向ベクトルがゼロか判定し、正規化時のゼロ除算を防ぐ値
constexpr auto kZeroOffset          =   0.0;                                    // 小立方体の頂点が各軸の最小側にあるときの座標オフセット
constexpr auto kFullOccupancy       =   1.0F;                                   // セル全体が内側の場合の占有率と、占有率の上限
constexpr auto kCellSize            =   1.0;                                    // 占有率を計算するセルの一辺の長さ（m）
constexpr auto kHalf                =   0.5;                                    // 半径への変換、中央の検査点、および小立方体の分割に使う比率
constexpr auto kCornersPerAxis      =   2;                                      // 立方体の各軸で最小側・最大側を列挙する数
constexpr auto kProbePointsPerAxis  =   3;                                      // 補助検査で各軸の最小・中央・最大を調べる数
constexpr auto kMiddleProbeIndex    =   1;                                      // 補助検査における中央点の添字。全軸端点の重複検査を除く
constexpr auto kXVertexMask         =   std::size_t{1} << axes::kAxisXIndex;    // 頂点番号から X 軸の最大側を判定するビット
constexpr auto kYVertexMask         =   std::size_t{1} << axes::kAxisYIndex;    // 頂点番号から Y 軸の最大側を判定するビット
constexpr auto kZVertexMask         =   std::size_t{1} << axes::kAxisZIndex;    // 頂点番号から Z 軸の最大側を判定するビット

/**
 * @brief ワールド座標上の位置を表す型。
 */
using Point = std::array<double, axes::kAxisCount>;

/**
 * @brief ワールド座標上の風向や位置差分を表す型。
 */
using WindVector = std::array<double, axes::kAxisCount>;

/**
 * @brief 形状判定に使う設定値がすべて有限値か確認する。
 * @param i_motor 確認する発生器の設定。
 * @return 位置、方向、寸法がすべて有限値の場合は true。
 */
bool HasFiniteShapeSettings(const motor::Settings& i_motor)
{
    const auto finite = [](const auto i_value) { return std::isfinite(i_value); };
    return std::all_of(i_motor.position.begin(), i_motor.position.end(), finite) &&
           std::all_of(i_motor.direction.begin(), i_motor.direction.end(), finite) &&
           finite(i_motor.diameter) && finite(i_motor.length);
}

/**
 * @brief 発生器の形状が占有率計算に対応しているか確認する。
 * @param i_shape 確認する発生器の形状。
 * @return 球、円錐、円柱のいずれかの場合は true。
 */
bool IsSupportedShape(const motor::Shape i_shape)
{
    return i_shape == motor::Shape::Sphere ||
           i_shape == motor::Shape::Cone ||
           i_shape == motor::Shape::Cylinder;
}

/**
 * @brief 形状に必要な寸法が最小値以上か確認する。
 * @param i_motor 確認する発生器の設定。
 * @return 直径と、球以外では長さが最小値以上の場合は true。
 */
bool HasValidDimensions(const motor::Settings& i_motor)
{
    return i_motor.diameter >= motor::defs::kMinimumSize &&
           (i_motor.shape == motor::Shape::Sphere ||
            i_motor.length >= motor::defs::kMinimumSize);
}

/**
 * @brief 発生位置と方向を使いやすい形にまとめる。
 */
struct ShapeData
{
    motor::Shape    shape;      // 発生器形状
    Point           position;   // 発生器の位置
    WindVector      direction;  // 発生器の方向（長さ 1）
    double          radius;     // 発生器の半径
    double          length;     // 発生器の長さ
};

/**
 * @brief 形状に必要な設定を検証し、方向を長さ 1 にする。
 * @param i_motor 風発生器の設定。
 * @return 形状の判定に必要な値。
 * @throws std::invalid_argument 形状に使えない値が指定された場合。
 */
ShapeData MakeShape(const motor::Settings& i_motor)
{
    // 非有限値は内外判定や補間を壊すため、形状に用いる値を先に検証する。
    // 球では長さの下限を適用しないが、長さ自体は有限であることを要求する。
    if (!HasFiniteShapeSettings(i_motor))
    {
        throw std::invalid_argument("発生器の形状または寸法が不正です。");
    }
    if (!IsSupportedShape(i_motor.shape) || !HasValidDimensions(i_motor))
    {
        throw std::invalid_argument("発生器の形状または寸法が不正です。");
    }

    // 球でも方向を含む設定として検証する。倍精度の hypot で長さを求め、
    // ゼロベクトルを除外してから正規化することで、後の軸方向の射影に使えるようにする。
    const auto directionLength = std::hypot(static_cast<double>(i_motor.direction[axes::kAxisXIndex]),
                                            static_cast<double>(i_motor.direction[axes::kAxisYIndex]),
                                            static_cast<double>(i_motor.direction[axes::kAxisZIndex]));
    if (directionLength == kZeroDirectionLength)
    {
        throw std::invalid_argument("発生器の方向にはゼロでないベクトルを指定してください。");
    }
    // 以降の形状判定では直径でなく半径を使用し、座標と寸法の計算を倍精度で行う。
    return {i_motor.shape,
            {
                i_motor.position[axes::kAxisXIndex],
                i_motor.position[axes::kAxisYIndex],
                i_motor.position[axes::kAxisZIndex]
            },
            {
                i_motor.direction[axes::kAxisXIndex] / directionLength,
                i_motor.direction[axes::kAxisYIndex] / directionLength,
                i_motor.direction[axes::kAxisZIndex] / directionLength
            },
            static_cast<double>(i_motor.diameter) * kHalf,
            static_cast<double>(i_motor.length)};
}

/**
 * @brief   指定位置で発生器形状の境界関数を評価する。
 * @param   i_shape 判定に用いる形状。
 * @param   i_point 判定するワールド座標。
 * @return  範囲内は負、境界上は 0、範囲外は正の値。
 * @details 球では境界までの符号付き距離になるが、円錐と円柱では厳密な距離とは限らない。
 */
double EvaluateShapeField(const ShapeData& i_shape, const Point& i_point)
{
    // 発生器の位置から判定点へ向かう相対位置ベクトルを求める。
    const auto delta = WindVector{
                                  i_point[axes::kAxisXIndex] - i_shape.position[axes::kAxisXIndex],
                                  i_point[axes::kAxisYIndex] - i_shape.position[axes::kAxisYIndex],
                                  i_point[axes::kAxisZIndex] - i_shape.position[axes::kAxisZIndex]
                                };
    
    // 球形風発生器
    if (i_shape.shape == motor::Shape::Sphere)
    {
        // 球は中心からの距離が半径以下なら内側。境界値の符号を内外判定に使う。
        return std::hypot(delta[axes::kAxisXIndex], delta[axes::kAxisYIndex], delta[axes::kAxisZIndex]) - i_shape.radius;
    }

    // 正規化済み方向との内積で軸方向の距離を求め、差分から軸方向成分を除いて
    // 軸に垂直な成分を得る。円柱と円錐は発生位置から前方へ伸びる。
    const auto along = delta[axes::kAxisXIndex] * i_shape.direction[axes::kAxisXIndex] +
                       delta[axes::kAxisYIndex] * i_shape.direction[axes::kAxisYIndex] +
                       delta[axes::kAxisZIndex] * i_shape.direction[axes::kAxisZIndex];
    const auto radial = WindVector{
        delta[axes::kAxisXIndex] - along * i_shape.direction[axes::kAxisXIndex],
        delta[axes::kAxisYIndex] - along * i_shape.direction[axes::kAxisYIndex],
        delta[axes::kAxisZIndex] - along * i_shape.direction[axes::kAxisZIndex]
    };

    // 円錐は先端から終端に向かって半径が線形に増え、円柱は半径が一定。
    // 側面・始端・終端の各条件の最大値を使うと、全条件を満たす位置が 0 以下になる。
    // この値は内外判定と辺の補間に使うもので、形状境界までの厳密な距離とは限らない。
    const auto radius = i_shape.shape == motor::Shape::Cone ? 
                        i_shape.radius * (along / i_shape.length) : i_shape.radius;
    return std::max({std::hypot(radial[axes::kAxisXIndex], radial[axes::kAxisYIndex], radial[axes::kAxisZIndex]) - radius,
                     -along, along - i_shape.length});
}

/**
 * @brief 小立方体を必要に応じてさらに分け、内側に重なる体積を求める
 * @param i_shape       判定する発生器
 * @param i_min         小立方体の最小座標
 * @param i_size        小立方体の辺の長さ
 * @param i_checkHidden 頂点に現れない重なりを一段階の補助分割で探す場合は true。
 * @return 小立方体に含まれる近似体積
 */
double CubeVolume(const ShapeData&  i_shape,
                  const Point&      i_min,
                  const double      i_size,
                  const bool        i_checkHidden)
{
    // 頂点番号の各ビットを X・Y・Z の最小側／最大側に対応させる。
    // マーチングキューブに渡す順序で境界値を保存し、境界上の頂点も内側として数える。
    auto values         =   std::array<double, cube::kVertexCount>{};
    auto insideCount    =   std::size_t{};
    for (auto index     =   std::size_t{}; index < values.size(); ++index)
    {
        // 頂点の座標を計算し、発生器との距離を計算する。
        values[index] = EvaluateShapeField(i_shape,
                                           {
                                                i_min[axes::kAxisXIndex] + ((index & kXVertexMask) ? i_size : kZeroOffset),
                                                i_min[axes::kAxisYIndex] + ((index & kYVertexMask) ? i_size : kZeroOffset),
                                                i_min[axes::kAxisZIndex] + ((index & kZVertexMask) ? i_size : kZeroOffset)
                                           });
        if (values[index] <= kSurfaceThreshold) { ++insideCount; }
    }
    const auto cubeVolume = i_size * i_size * i_size;

    // 対象の形状は凸なので、全頂点が内側なら立方体全体が内側になり、発生器の重なりは100%になる。
    if (insideCount == values.size()) { return cubeVolume; }

    // 全頂点が外側の場合は、形状が立方体に入り込んでいるかどうかを補助分割で確認する
    if (insideCount == 0)
    {
        // 補助分割後は再帰的な探索を行わず、全頂点が外側の子立方体を空として扱う。
        if (!i_checkHidden) { return kEmptyVolume; }

        // 8 頂点が外側でも風発生器が中へ入り込むことがあるため、各軸の最小・中央・最大を組み合わせる。
        // 全軸が端の組み合わせは既に調べた頂点なので除き、辺・面・立方体の中央を探す。
        // ここにも現れない細い重なりは検出できない。
        auto found = false;
        for (auto z = 0; z < kProbePointsPerAxis && !found; ++z)
        {
            for (auto y = 0; y < kProbePointsPerAxis && !found; ++y)
            {
                for (auto x = 0; x < kProbePointsPerAxis && !found; ++x)
                {
                    if (x != kMiddleProbeIndex && y != kMiddleProbeIndex && z != kMiddleProbeIndex) { continue; }
                    found = EvaluateShapeField(i_shape,
                                               {i_min[axes::kAxisXIndex] + x * i_size * kHalf,
                                                i_min[axes::kAxisYIndex] + y * i_size * kHalf,
                                                i_min[axes::kAxisZIndex] + z * i_size * kHalf}) <= kSurfaceThreshold;
                }
            }
        }
        if (!found) { return kEmptyVolume; }

        // 見つかった位置を子立方体の頂点に含められるよう、各辺を半分にして体積を合計する。
        // false を渡して補助分割は一段階に制限する。
        auto volume = kEmptyVolume;
        const auto half = i_size * kHalf;
        for (auto z = 0; z < kCornersPerAxis; ++z)
        {
            for (auto y = 0; y < kCornersPerAxis; ++y)
            {
                for (auto x = 0; x < kCornersPerAxis; ++x)
                {
                    // 八分割した箱で再帰的に体積を求める
                    volume += CubeVolume(i_shape, {i_min[axes::kAxisXIndex] + x * half,
                                                   i_min[axes::kAxisYIndex] + y * half,
                                                   i_min[axes::kAxisZIndex] + z * half}, half, false);
                }
            }
        }
        return volume;
    }

    // 面の中心を X 最小・最大、Y 最小・最大、Z 最小・最大の順に評価する。
    // 対角に内側の頂点がある面は中心の内外で交点の接続を決めるため、
    // 隣り合う立方体が共有面で同じ境界を作れる。
    const auto half =   i_size * kHalf;
    const auto x    =   i_min[axes::kAxisXIndex];
    const auto y    =   i_min[axes::kAxisYIndex];
    const auto z    =   i_min[axes::kAxisZIndex];

    // 各面の中心座標を求める
    const auto centers = std::array<Point, cube::kFaceCount>{{
        {x, y + half, z + half},
        {x + i_size, y + half, z + half},
        {x + half, y, z + half},
        {x + half, y + i_size, z + half},
        {x + half, y + half, z},
        {x + half, y + half, z + i_size}}};

    // 各面の中心で境界値を評価する
    auto faceValues = std::array<double, cube::kFaceCount>{};
    for (auto index = std::size_t{}; index < centers.size(); ++index)
    {
        faceValues[index] = EvaluateShapeField(i_shape, centers[index]);
    }
    // 単位立方体に対する占有体積を実際の小立方体の体積に換算して近似します
    return detail::MarchingCubeVolume(values, faceValues) * cubeVolume;
}
}

detail::ShapeRegion::ShapeRegion(const motor::Settings& i_motor)
{
    const auto shape = MakeShape(i_motor);
    m_eShape = shape.shape;
    m_stPosition = shape.position;
    m_stDirection = shape.direction;
    m_dRadius = shape.radius;
    m_dLength = shape.length;
}

detail::RegionRelation detail::ShapeRegion::Classify(const std::array<double, 3>& i_min,
                                                      const std::array<double, 3>& i_max) const
{
    const auto shape = ShapeData{m_eShape, m_stPosition, m_stDirection, m_dRadius, m_dLength};
    auto center = Point{};
    auto half = Point{};
    auto scale = std::max({1.0, std::abs(m_dRadius), std::abs(m_dLength)});
    for (auto axis = std::size_t{}; axis < center.size(); ++axis)
    {
        center[axis] = i_min[axis] + (i_max[axis] - i_min[axis]) * 0.5;
        half[axis] = (i_max[axis] - i_min[axis]) * 0.5;
        scale = std::max({scale, std::abs(i_min[axis]), std::abs(i_max[axis]), std::abs(m_stPosition[axis])});
    }

    // 円錐の側面関数は距離そのものではないため、傾きに応じて変化量の上限を広げる。
    const auto lipschitz = m_eShape == motor::Shape::Cone ? std::hypot(1.0, m_dRadius / m_dLength) : 1.0;
    const auto reach = lipschitz * std::hypot(half[0], half[1], half[2]);
    // 境界付近では座標の丸めと円錐の傾きを考慮し、内外を断定しない。
    const auto tolerance = 128.0 * std::numeric_limits<double>::epsilon() *
                           std::max({scale * lipschitz, reach, 1.0});
    if (EvaluateShapeField(shape, center) > reach + tolerance) { return RegionRelation::Outside; }

    // 形状は凸なので、直方体の全頂点が十分に内側なら領域全体も内側になる。
    for (auto z = 0; z < 2; ++z)
    {
        for (auto y = 0; y < 2; ++y)
        {
            for (auto x = 0; x < 2; ++x)
            {
                if (EvaluateShapeField(shape, {x == 0 ? i_min[0] : i_max[0],
                                              y == 0 ? i_min[1] : i_max[1],
                                              z == 0 ? i_min[2] : i_max[2]}) >= -tolerance)
                {
                    return RegionRelation::Partial;
                }
            }
        }
    }
    return RegionRelation::Inside;
}

/**
 * @brief   一辺 1 m のセルと風発生器の重なりを求める。
 * @param   i_motor     風発生器の設定。無効状態や風速は形状の判定に影響しない。
 * @param   i_cellMin   セルの最小座標。ワールド軸に沿った位置（m）。
 * @param   i_settings  境界付近の近似精度。
 * @return  重なった体積の割合。0 以上 1 以下。
 * @throws  std::invalid_argument 形状、寸法、方向、座標または分割数が不正な場合。
 * @throws  std::logic_error 境界面を閉じられない、または計算した体積が立方体の範囲外の場合。
 * @details
 * セルの全頂点が形状内にある場合は 1 を返す。それ以外は小立方体ごとに、
 * 辺の交点をつないだ境界面と立方体の外面で囲まれた体積を足し合わせる。
 * 頂点に現れない重なりは辺・面・立方体の中央も調べ、見つかれば一段階細分化する。
 * それらの位置にも現れない細い重なりは見逃す場合がある。
 * 対角の内側を持つ面は面の中心で接続方法を決め、複数の境界輪郭はそれぞれ独立に閉じる。
 */
float CalculateOccupancy(const motor::Settings&                     i_motor,
                         const std::array<float, axes::kAxisCount>& i_cellMin,
                         const OccupancySettings&                   i_settings)
{
    // 分割数やセル座標の不正を先に拒否し、計算結果の 0% と入力エラーを区別する。
    if (i_settings.subdivisions < defs::kMinimumSubdivisions ||
        i_settings.subdivisions > defs::kMaximumSubdivisions ||
        !std::all_of(i_cellMin.begin(), i_cellMin.end(), [](const auto i_value) { return std::isfinite(i_value); }))
    {
        throw std::invalid_argument("セルの位置または占有率の分割数が不正です。");
    }

    // 発生器の寸法と方向も検証し、以降の判定に使う正規化済み形状を用意する。
    const auto shape = MakeShape(i_motor);

    // 対象はすべて凸形状なので、セルの全頂点が内側または境界上なら
    // セル全体も含まれる。細分化とマーチングキューブを省いて 100% を返せる。
    auto fullyContained = true;
    for (auto z = 0; z < kCornersPerAxis; ++z)
    {
        for (auto y = 0; y < kCornersPerAxis; ++y)
        {
            for (auto x = 0; x < kCornersPerAxis; ++x)
            {
                fullyContained = fullyContained && EvaluateShapeField(shape, {
                    static_cast<double>(i_cellMin[axes::kAxisXIndex]) + x * kCellSize,
                    static_cast<double>(i_cellMin[axes::kAxisYIndex]) + y * kCellSize,
                    static_cast<double>(i_cellMin[axes::kAxisZIndex]) + z * kCellSize}) <= kSurfaceThreshold;
            }
        }
    }
    // 全部なら100%を返す
    if (fullyContained) { return kFullOccupancy; }

    // 各軸を指定回数に等分し、小立方体の最小座標と辺長から近似体積を求める。
    // 各小立方体では頂点に現れない重なりも一段階の補助分割で探す。
    auto volume         =   kEmptyVolume;
    const auto count    =   i_settings.subdivisions;
    const auto step     =   kCellSize / count;
    auto region = std::optional<detail::ShapeRegion>{};
    // 球は小立方体の予備判定より既存の計算が速いため、円柱と円錐だけで使う。
    if (i_motor.shape != motor::Shape::Sphere) { region.emplace(i_motor); }
    for (auto z = 0; z < count; ++z)
    {
        for (auto y = 0; y < count; ++y)
        {
            for (auto x = 0; x < count; ++x)
            {
                const auto point = Point{
                    static_cast<double>(i_cellMin[axes::kAxisXIndex]) + x * step,
                    static_cast<double>(i_cellMin[axes::kAxisYIndex]) + y * step,
                    static_cast<double>(i_cellMin[axes::kAxisZIndex]) + z * step};
                if (region)
                {
                    const auto relation = region->Classify(point, {point[0] + step, point[1] + step, point[2] + step});
                    if (relation == detail::RegionRelation::Outside) { continue; }
                    if (relation == detail::RegionRelation::Inside)
                    {
                        volume += step * step * step;
                        continue;
                    }
                }
                volume += CubeVolume(shape, point, step, true);
            }
        }
    }
    
    // セルは一辺 1 m なので体積の合計がそのまま占有率になる。
    // 浮動小数点演算による範囲外へのわずかなずれは 0～1 に収める。
    return static_cast<float>(std::clamp(volume, kEmptyVolume, static_cast<double>(kFullOccupancy)));
}
}
