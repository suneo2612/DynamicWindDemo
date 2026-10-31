/**
 * @file motor_field.cpp
 * @brief 発生器の占有率、方向と距離減衰から風の寄与を準備する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "motor_field.h"
#include "occupancy_region.h"
#include "../defs/wind_region_constants.h"
#include "../motor/defs/wind_motor_constraints.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace demo::wind::simulation
{
namespace windDefs = demo::wind::defs;

namespace
{
/**
 * @brief ワールド座標や方向を倍精度の XYZ 成分で表す。
 */
using Point = std::array<double, windDefs::kAxisCount>;

/**
 * @brief 発生器の各セルへの寄与を算出し、同じ種類の風を合計する。
 * @param i_motor 寄与を計算する発生器の設定。
 * @param i_origin 管理空間の最小座標。各軸とも整数メートル。
 * @param i_size X、Y、Z 順のセル数。
 * @param i_settings セルと発生器の重なりを近似する精度。
 * @param io_cells 加算先のセル。X が最も速く変化する順序。
 * @pre 各軸のセル数は正であり、io_cells はその積に等しい要素数を持つ。
 * @throws std::invalid_argument 発生器の形状、寸法、方向、風速または発生方法が不正な場合。
 * @throws std::overflow_error セルに加算した風の成分が表現範囲を超えた場合。
 * @throws std::logic_error 占有率の近似計算で境界や体積を求められない場合。
 * @details 管理空間外や無効な発生器でも設定を検証する。領域を分割して形状の内外を判定し、
 * 境界にあるセルだけ占有率を近似する。失敗時には io_cells の一部が更新済みの場合がある。
 */
void AddMotor(const motor::Settings& i_motor,
              const std::array<int, windDefs::kAxisCount>& i_origin,
              const std::array<int, windDefs::kAxisCount>& i_size,
              const OccupancySettings& i_settings,
              std::vector<MotorCell>& io_cells)
{
    // 形状検証を先に行い、範囲外の発生器でも不正な設定を見逃さない。
    const auto region = detail::ShapeRegion(i_motor);
    if (!std::isfinite(i_motor.speed) || i_motor.speed < 0.0F ||
        i_motor.speed > motor::defs::kMaximumSpeed ||
        (i_motor.mode != motor::Mode::Continuous && i_motor.mode != motor::Mode::Instant))
    {
        throw std::invalid_argument("発生器の風速または発生方法が不正です。");
    }

    // 風速, 風方向ベクトル
    const auto length = std::hypot(static_cast<double>(i_motor.direction[windDefs::kAxisXIndex]),
                                   static_cast<double>(i_motor.direction[windDefs::kAxisYIndex]),
                                   static_cast<double>(i_motor.direction[windDefs::kAxisZIndex]));
    const auto direction = Point{i_motor.direction[windDefs::kAxisXIndex] / length,
                                 i_motor.direction[windDefs::kAxisYIndex] / length,
                                 i_motor.direction[windDefs::kAxisZIndex] / length};
    
    // 半径
    const auto  radius  =   static_cast<double>(i_motor.diameter) * 0.5;

    // 形状を囲む直方体と管理空間の重なりだけを調べる。円錐も円柱を囲む範囲を使う。
    auto    first   =   std::array<int, windDefs::kAxisCount>{};
    auto    last    =   std::array<int, windDefs::kAxisCount>{};
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {   
        // 球の場合は start と end が同じになるが、円柱や円錐の場合は方向ベクトルに沿って end を計算する。
        const auto start    =   static_cast<double>(i_motor.position[axis]);
        const auto end      =   i_motor.shape == motor::Shape::Sphere ? start : start + direction[axis] * static_cast<double>(i_motor.length);

        // 管理空間の原点から見た、形状を囲むセル範囲の下限と上限（上限は含まない）。
        const auto low      =   std::floor(std::min(start, end) - radius - i_origin[axis]);
        const auto high     =   std::ceil(std::max(start, end) + radius - i_origin[axis]);

        // セル範囲を管理空間の範囲内に制限する。
        first[axis]         =   static_cast<int>(std::clamp(low, 0.0, static_cast<double>(i_size[axis])));
        last[axis]          =   static_cast<int>(std::clamp(high, 0.0, static_cast<double>(i_size[axis])));
    }
    if (!i_motor.enabled || i_motor.speed == 0.0F) { return; }

    // 形状を囲むセル範囲が存在しない場合は処理を中止する。
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        if (first[axis] == last[axis]) { return; }
    }

    /**
     * @brief セル中心の風向きと距離減衰を求め、占有率に応じた寄与を加算する。
     * @details i_x、i_y、i_z は管理空間内のセル座標、i_occupancy はセルの占有率。
     */
    const auto addCell = [&](const int i_x, const int i_y, const int i_z, const float i_occupancy)
    {
        if (i_occupancy == 0.0F) { return; }
        const auto cell = std::array<float, windDefs::kAxisCount>{
            static_cast<float>(static_cast<double>(i_origin[windDefs::kAxisXIndex]) + i_x),
            static_cast<float>(static_cast<double>(i_origin[windDefs::kAxisYIndex]) + i_y),
            static_cast<float>(static_cast<double>(i_origin[windDefs::kAxisZIndex]) + i_z)};
        const auto delta = Point{
            static_cast<double>(cell[windDefs::kAxisXIndex]) + 0.5 - i_motor.position[windDefs::kAxisXIndex],
            static_cast<double>(cell[windDefs::kAxisYIndex]) + 0.5 - i_motor.position[windDefs::kAxisYIndex],
            static_cast<double>(cell[windDefs::kAxisZIndex]) + 0.5 - i_motor.position[windDefs::kAxisZIndex]};
        const auto distance = std::hypot(delta[windDefs::kAxisXIndex], delta[windDefs::kAxisYIndex],
                                         delta[windDefs::kAxisZIndex]);
        // 球と円錐は放射方向、円柱は設定方向を使う。
        auto windDirection = direction;
        if (i_motor.shape != motor::Shape::Cylinder && distance > 0.0)
        {
            for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
            {
                windDirection[axis] = delta[axis] / distance;
            }
        }

        // 球は中心からの距離、円錐と円柱は軸方向の距離で減衰させる。
        auto falloff = 1.0;
        if (i_motor.distanceFalloff)
        {
            const auto along = delta[windDefs::kAxisXIndex] * direction[windDefs::kAxisXIndex] +
                               delta[windDefs::kAxisYIndex] * direction[windDefs::kAxisYIndex] +
                               delta[windDefs::kAxisZIndex] * direction[windDefs::kAxisZIndex];
            const auto ratio = i_motor.shape == motor::Shape::Sphere
                ? distance / radius : along / static_cast<double>(i_motor.length);
            falloff = std::clamp(1.0 - ratio, 0.0, 1.0);
        }
        const auto speed = static_cast<float>(i_motor.speed * i_occupancy * falloff);
        const auto index = static_cast<std::size_t>(i_x) + static_cast<std::size_t>(i_size[windDefs::kAxisXIndex]) *
            (static_cast<std::size_t>(i_y) + static_cast<std::size_t>(i_size[windDefs::kAxisYIndex]) * i_z);
        auto& cellWind = io_cells[index];
        auto& sum = i_motor.mode == motor::Mode::Continuous ? cellWind.continuous : cellWind.instant;
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            sum[axis] += static_cast<float>(speed * windDirection[axis]);
            if (!std::isfinite(sum[axis])) { throw std::overflow_error("発生器の風の合計が範囲外です。"); }
        }
        if (i_motor.mode == motor::Mode::Continuous) { sum[3] = std::max(sum[3], speed); }
    };

    /**
     * @brief セル領域を再帰的に分類し、形状に重なるセルの寄与を加算する。
     * @details i_first は開始座標、i_last は終了座標の直後を表す。i_self は再帰呼び出しに使う。
     */
    const auto visit = [&](const auto& i_self, const std::array<int, windDefs::kAxisCount>& i_first,
                           const std::array<int, windDefs::kAxisCount>& i_last) -> void
    {
        auto min = Point{};
        auto max = Point{};
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            min[axis] = static_cast<double>(i_origin[axis]) + i_first[axis];
            max[axis] = static_cast<double>(i_origin[axis]) + i_last[axis];
        }
        const auto relation = region.Classify(min, max);
        if (relation == detail::RegionRelation::Outside) { return; }
        if (relation == detail::RegionRelation::Inside)
        {
            for (auto z = i_first[windDefs::kAxisZIndex]; z < i_last[windDefs::kAxisZIndex]; ++z)
            {
                for (auto y = i_first[windDefs::kAxisYIndex]; y < i_last[windDefs::kAxisYIndex]; ++y)
                {
                    for (auto x = i_first[windDefs::kAxisXIndex]; x < i_last[windDefs::kAxisXIndex]; ++x)
                    {
                        addCell(x, y, z, 1.0F);
                    }
                }
            }
            return;
        }
        if (i_last[windDefs::kAxisXIndex] - i_first[windDefs::kAxisXIndex] == 1 &&
            i_last[windDefs::kAxisYIndex] - i_first[windDefs::kAxisYIndex] == 1 &&
            i_last[windDefs::kAxisZIndex] - i_first[windDefs::kAxisZIndex] == 1)
        {
            const auto cell = std::array<float, windDefs::kAxisCount>{
                static_cast<float>(min[windDefs::kAxisXIndex]),
                static_cast<float>(min[windDefs::kAxisYIndex]),
                static_cast<float>(min[windDefs::kAxisZIndex])};
            addCell(i_first[windDefs::kAxisXIndex], i_first[windDefs::kAxisYIndex],
                    i_first[windDefs::kAxisZIndex], CalculateOccupancy(i_motor, cell, i_settings));
            return;
        }
        auto middle = std::array<int, windDefs::kAxisCount>{};
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            const auto span = i_last[axis] - i_first[axis];
            middle[axis] = i_first[axis] + span / 2 + span % 2;
        }
        for (auto z = 0; z < (middle[windDefs::kAxisZIndex] < i_last[windDefs::kAxisZIndex] ? 2 : 1); ++z)
        {
            for (auto y = 0; y < (middle[windDefs::kAxisYIndex] < i_last[windDefs::kAxisYIndex] ? 2 : 1); ++y)
            {
                for (auto x = 0; x < (middle[windDefs::kAxisXIndex] < i_last[windDefs::kAxisXIndex] ? 2 : 1); ++x)
                {
                    i_self(i_self, {x == 0 ? i_first[windDefs::kAxisXIndex] : middle[windDefs::kAxisXIndex],
                                    y == 0 ? i_first[windDefs::kAxisYIndex] : middle[windDefs::kAxisYIndex],
                                    z == 0 ? i_first[windDefs::kAxisZIndex] : middle[windDefs::kAxisZIndex]},
                           {x == 0 ? middle[windDefs::kAxisXIndex] : i_last[windDefs::kAxisXIndex],
                            y == 0 ? middle[windDefs::kAxisYIndex] : i_last[windDefs::kAxisYIndex],
                            z == 0 ? middle[windDefs::kAxisZIndex] : i_last[windDefs::kAxisZIndex]});
                }
            }
        }
    };
    visit(visit, first, last);
}

}

/**
 * @brief 発生器の一覧から、管理空間の各セルに加える風を準備する。
 * @param i_motors 計算対象の発生器一覧。
 * @param i_origin 管理空間の最小座標。各軸とも整数メートル。
 * @param i_size X、Y、Z 順のセル数。
 * @param i_settings セルと発生器の重なりを近似する精度。
 * @return 各セルの寄与と、有効な瞬間発生器の識別子。管理空間外の発生器も識別子に含む。
 * @throws std::invalid_argument 管理空間、発生器または占有率設定が不正な場合。
 * @throws std::overflow_error セル数または風の合計が表現範囲を超えた場合。
 * @throws std::logic_error 占有率の近似計算で境界や体積を求められない場合。
 * @details 方向と距離減衰はセル中心で評価する。発生器の解放は行わない。
 */
MotorField BuildMotorField(const std::vector<motor::Motor>& i_motors,
                           const std::array<int, windDefs::kAxisCount>& i_origin,
                           const std::array<int, windDefs::kAxisCount>& i_size,
                           const OccupancySettings& i_settings)
{
    // 占有率計算が対応する分割数の範囲内か確認する。
    if (i_settings.subdivisions < defs::kMinimumSubdivisions ||
        i_settings.subdivisions > defs::kMaximumSubdivisions)
    {
        throw std::invalid_argument("占有率の分割数が不正です。");
    }

    // 各軸のセル数を掛け合わせ、結果のセル配列に必要な要素数を求める。
    auto count = std::size_t{1};
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        // float で隣り合う整数を区別できる座標範囲に管理空間を制限する。
        const auto end = static_cast<double>(i_origin[axis]) + i_size[axis];
        if (i_size[axis] <= 0 || i_origin[axis] < -16777216 || end > 16777216.0)
        {
            throw std::invalid_argument("管理空間の寸法または座標が不正です。");
        }

        // 乗算前に上限を確認し、セル数の計算で size_t があふれるのを防ぐ。
        if (count > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(i_size[axis]))
        {
            throw std::overflow_error("管理空間のセル数が範囲外です。");
        }
        count *= static_cast<std::size_t>(i_size[axis]);
    }

    // 全セルの寄与をゼロ初期化し、発生器ごとに寄与を集計する。
    auto result = MotorField{};
    result.cells.resize(count);
    for (const auto& motor : i_motors)
    {
        AddMotor(motor.settings, i_origin, i_size, i_settings, result.cells);

        // 瞬間発生器は管理空間外でも、更新完了後に解放するため識別子を記録する。
        if (motor.settings.enabled && motor.settings.mode == motor::Mode::Instant)
        {
            result.instantIds.push_back(motor.id);
        }
    }
    return result;
}

/**
 * @brief キャッシュ済み入力と今回の条件を比較する。
 * @param i_motors 今回の発生器一覧。
 * @param i_origin 管理空間の最小座標。
 * @param i_size 管理空間の各軸のセル数。
 * @param i_settings 占有率の近似設定。
 * @return キャッシュが存在し、入力条件がすべて一致する場合は true。
 */
bool MotorFieldCache::HasMatchingInput(const std::vector<motor::Motor>& i_motors,
                                       const std::array<int, 3>& i_origin,
                                       const std::array<int, 3>& i_size,
                                       const OccupancySettings& i_settings) const
{
    return  m_instEntry &&
            m_instEntry->motors == i_motors &&
            m_instEntry->origin == i_origin &&
            m_instEntry->size == i_size &&
            m_instEntry->subdivisions == i_settings.subdivisions;
}

/**
 * @brief 入力が前回と同じ場合は保存済みの寄与を返し、異なる場合は再計算する。
 * @param i_motors 計算対象の発生器一覧。
 * @param i_origin 管理空間の最小座標。
 * @param i_size X、Y、Z 順のセル数。
 * @param i_settings セルと発生器の重なりを近似する精度。
 * @return 寄与への参照と再計算の有無。参照は次の再計算またはキャッシュの破棄まで有効。
 * @throws std::invalid_argument 管理空間、発生器または占有率設定が不正な場合。
 * @throws std::overflow_error セル数または風の合計が表現範囲を超えた場合。
 * @throws std::logic_error 占有率の近似計算で境界や体積を求められない場合。
 */
MotorFieldCache::Result MotorFieldCache::Get(std::vector<motor::Motor> i_motors,
                                             const std::array<int, windDefs::kAxisCount>& i_origin,
                                             const std::array<int, windDefs::kAxisCount>& i_size,
                                             const OccupancySettings& i_settings)
{
    if (HasMatchingInput(i_motors, i_origin, i_size, i_settings))
    {
        return {m_instEntry->field, false};
    }

    auto field = BuildMotorField(i_motors, i_origin, i_size, i_settings);
    m_instEntry = Entry{std::move(i_motors), i_origin, i_size, i_settings.subdivisions, std::move(field)};
    return {m_instEntry->field, true};
}

}   // namespace demo::wind::simulation