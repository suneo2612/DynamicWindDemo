/**
 * @file wind_region.cpp
 * @brief 風の管理空間の位置と視線への追従を実装する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#include "wind_region.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace demo::wind
{

namespace
{

/**
 * @brief   座標配列の全要素が有限値か調べる。
 * @param   i_value 調べる座標配列。
 * @return  全要素が有限値の場合は true、それ以外は false。
 */
bool IsFinite(const std::array<float, defs::kAxisCount>& i_value)
{
    return std::all_of(
        i_value.begin(),
        i_value.end(),
        [](const auto i_component)
        {
            return std::isfinite(i_component);
        });
}

/**
 * @brief   軸の正負方向で閾値が管理空間の境界を超えるか調べる
 * @param   i_negativeDirectionThreshold    負方向の移動閾値
 * @param   i_positiveDirectionThreshold    正方向の移動閾値
 * @param   i_axisSize                      軸方向の管理空間の寸法
 * @param   i_referencePosition             軸方向の基準位置
 * @return  いずれかの閾値が境界までの距離を超える場合は true
 */
bool ExceedsRegionBoundary(const float  i_negativeDirectionThreshold,
                           const float  i_positiveDirectionThreshold,
                           const int    i_axisSize,
                           const float  i_referencePosition)
{
    // 基準位置から各方向の境界までの距離を求め、対応する移動閾値を超えるか判定する。
    const auto positiveDirectionDistance    =   static_cast<float>(i_axisSize) - i_referencePosition;
    return i_negativeDirectionThreshold > i_referencePosition ||
           i_positiveDirectionThreshold > positiveDirectionDistance;
}

/**
 * @brief 管理空間の追従設定が制約を満たすか検証する。
 * @param i_settings 寸法、基準位置、追従条件。
 * @throws std::invalid_argument 寸法、基準位置、閾値、座標系のいずれかが不正な場合。
 */
void Validate(const WindRegionSettings& i_settings)
{
    // 管理空間の寸法が最小値以上であることを保証
    if (i_settings.size[defs::kAxisXIndex] < defs::kMinimumSize[defs::kAxisXIndex] ||
        i_settings.size[defs::kAxisYIndex] < defs::kMinimumSize[defs::kAxisYIndex] ||
        i_settings.size[defs::kAxisZIndex] < defs::kMinimumSize[defs::kAxisZIndex] ||
        !IsFinite(i_settings.reference))
    {
        throw std::invalid_argument("管理空間の寸法または基準位置が不正です。");
    }

    // 基準位置が管理空間内に収まっていることを保証
    for (auto axis = std::size_t{}; axis < defs::kAxisCount; ++axis)
    {
        if (i_settings.reference[axis] < defs::kZero ||
            i_settings.reference[axis] >= static_cast<float>(i_settings.size[axis]))
        {
            throw std::invalid_argument("基準位置は管理空間内に指定してください。");
        }
    }

    // 追従条件が正の有限値であることを保証
    if (!std::isfinite(i_settings.forwardThreshold) || i_settings.forwardThreshold <= defs::kZero ||
        !std::isfinite(i_settings.backwardThreshold) || i_settings.backwardThreshold <= defs::kZero ||
        !std::isfinite(i_settings.lateralThreshold) || i_settings.lateralThreshold <= defs::kZero ||
        !std::isfinite(i_settings.verticalThreshold) || i_settings.verticalThreshold <= defs::kZero ||
        !std::isfinite(i_settings.rotationDegrees) || i_settings.rotationDegrees <= defs::kZero ||
        i_settings.rotationDegrees > defs::kDegreesPerHalfTurn ||
        (i_settings.coordinateSystem != CoordinateSystem::RightHanded &&
         i_settings.coordinateSystem != CoordinateSystem::LeftHanded))
    {
        throw std::invalid_argument("管理空間の追従条件が不正です。");
    }

    // 移動を検出する前に基準点が管理空間の外へ出る設定は受け付けないことを保証
    if (ExceedsRegionBoundary(
            i_settings.lateralThreshold,
            i_settings.lateralThreshold,
            i_settings.size[defs::kAxisXIndex],
            i_settings.reference[defs::kAxisXIndex]) ||
        ExceedsRegionBoundary(
            i_settings.backwardThreshold,
            i_settings.forwardThreshold,
            i_settings.size[defs::kAxisZIndex],
            i_settings.reference[defs::kAxisZIndex]) ||
        ExceedsRegionBoundary(
            i_settings.verticalThreshold,
            i_settings.verticalThreshold,
            i_settings.size[defs::kAxisYIndex],
            i_settings.reference[defs::kAxisYIndex]))
    {
        throw std::invalid_argument("追従距離が基準点から管理空間の端までの距離を超えています。");
    }
}

/**
 * @brief 視線方向から正規化した水平ベクトルを求める。
 * @param i_forward 現在の視線方向ベクトル。
 * @param i_previous 水平成分がほぼゼロの場合に使う前回の方向。
 * @return X-Z 平面上の水平単位ベクトル。
 * @throws std::invalid_argument 視線方向に有限でない値が含まれるか、ゼロベクトルの場合。
 * @details 水平成分の長さが最小値未満の場合は i_previous を返す。
 */
std::array<float, defs::kHorizontalDirectionCount> Horizontal(
    const std::array<float, defs::kAxisCount>&                  i_forward,
    const std::array<float, defs::kHorizontalDirectionCount>&   i_previous)
{
    if (!IsFinite(i_forward) ||
        (i_forward[defs::kAxisXIndex] == defs::kZero &&
         i_forward[defs::kAxisYIndex] == defs::kZero &&
         i_forward[defs::kAxisZIndex] == defs::kZero))
    {
        throw std::invalid_argument("視線方向は有限なゼロでないベクトルを指定してください。");
    }

    // 水平方向の長さを計算して正規化
    const auto length = std::hypot(static_cast<double>(i_forward[defs::kAxisXIndex]),
                                   static_cast<double>(i_forward[defs::kAxisZIndex]));
    if (length < defs::kMinimumHorizontalLength) { return i_previous; }
    return {static_cast<float>(i_forward[defs::kAxisXIndex] / length),
            static_cast<float>(i_forward[defs::kAxisZIndex] / length)};
}

/**
 * @brief   実数の移動量を整数のセル数に変換する。
 * @param   i_value 変換する移動量。
 * @return  端数をゼロ方向に切り捨てた整数のセル数。
 * @throws  std::overflow_error 値が有限でないか、int 型の表現範囲を超える場合。
 */
int ToCell(const double i_value)
{
    if (!std::isfinite(i_value) ||
        i_value < static_cast<double>(std::numeric_limits<int>::min()) ||
        i_value > static_cast<double>(std::numeric_limits<int>::max()))
    {
        throw std::overflow_error("管理空間の位置が表現範囲を超えました。");
    }
    return static_cast<int>(i_value);
}

/**
 * @brief 基準点と水平視線に対する管理空間の目標原点を求める。
 * @param i_settings    管理空間の寸法、基準点、座標系。
 * @param i_position    基準点のワールド座標。
 * @param i_forward     正規化済みの水平視線ベクトル。
 * @return  XYZ 各軸の目標原点。整数セル境界への丸めは呼び出し側で行う。
 */
std::array<double, defs::kAxisCount> DesiredOrigin(
    const WindRegionSettings& i_settings,
    const std::array<float, defs::kAxisCount>& i_position,
    const std::array<float, defs::kHorizontalDirectionCount>& i_forward)
{
    // 座標系に応じて水平方向の右向きベクトルを定める。
    const auto sign = i_settings.coordinateSystem == CoordinateSystem::RightHanded
        ? defs::kPositiveUnit
        : defs::kNegativeUnit;
    const auto rightX = -i_forward[defs::kHorizontalZIndex] * sign;
    const auto rightZ = i_forward[defs::kHorizontalXIndex] * sign;

    // 基準点から管理空間の中心までの右方向・前方向の距離を求める。
    const auto sideways = static_cast<double>(i_settings.size[defs::kAxisXIndex]) * defs::kHalf -
                          i_settings.reference[defs::kAxisXIndex];
    const auto forward = static_cast<double>(i_settings.size[defs::kAxisZIndex]) * defs::kHalf -
                         i_settings.reference[defs::kAxisZIndex];

    // 基準点に対する管理空間の中心位置から、寸法の半分を引いて原点を求める。
    return {
        static_cast<double>(i_position[defs::kAxisXIndex]) + rightX * sideways +
                            i_forward[defs::kHorizontalXIndex] * forward -
                            i_settings.size[defs::kAxisXIndex] * defs::kHalf,
        static_cast<double>(i_position[defs::kAxisYIndex]) -
                            i_settings.reference[defs::kAxisYIndex],
        static_cast<double>(i_position[defs::kAxisZIndex]) + rightZ * sideways +
                            i_forward[defs::kHorizontalZIndex] * forward -
                            i_settings.size[defs::kAxisZIndex] * defs::kHalf};
}
}

/**
 * @brief   設定と初期位置・視線から管理空間を初期化する。
 * @param   i_settings  寸法、座標系、追従条件。
 * @param   i_position  初期基準点のワールド座標。
 * @param   i_forward   初期視線方向。
 * @throws  std::invalid_argument 設定値、位置、視線方向のいずれかが不正な場合。
 * @throws  std::overflow_error 初期原点が int 型の表現範囲を超える場合。
 * @details 初期原点はワールド軸に沿った整数セル境界に合わせる。
 */
WindRegion::WindRegion(const WindRegionSettings& i_settings,
                       const std::array<float, defs::kAxisCount>& i_position,
                       const std::array<float, defs::kAxisCount>& i_forward)
    : m_stSettings(i_settings)
    , m_instLastPosition(i_position)
{
    Validate(i_settings);
    if (!IsFinite(i_position)) { throw std::invalid_argument("基準点は有限値を指定してください。"); }

    // 水平成分が得られない視線に備え、座標系に応じた初期前方を用意する。
    const auto initial = std::array<float, defs::kHorizontalDirectionCount>{
        defs::kZero,
        i_settings.coordinateSystem == CoordinateSystem::RightHanded
            ? defs::kNegativeUnit
            : defs::kPositiveUnit};
    m_instForward = Horizontal(i_forward, initial);

    // 初期視線を比較基準にも設定し、最初の更新で不要な回転判定を起こさない。
    m_instLastForward = m_instForward;

    // 目標原点を各軸の整数セル境界に切り下げ、ワールド軸に沿って初期配置する。
    const auto desired = DesiredOrigin(m_stSettings, i_position, m_instForward);
    for (auto axis = std::size_t{}; axis < defs::kAxisCount; ++axis)
    {
        m_instOrigin[axis] = ToCell(std::floor(desired[axis]));
    }
}

/**
 * @brief   基準点と視線を更新し、必要なら管理空間を移動する。
 * @param   i_position 現在の基準点のワールド座標。
 * @param   i_forward 現在の視線方向。
 * @return  更新後の原点、移動セル数、更新の有無。
 * @throws  std::invalid_argument 位置または視線方向が不正な場合。
 * @throws  std::overflow_error 原点または移動量が int 型の表現範囲を超える場合。
 * @details 移動条件を満たさない場合は原点を維持し、updated を false にする。
 */
WindRegionMove WindRegion::Update(const std::array<float, defs::kAxisCount>& i_position,
                                  const std::array<float, defs::kAxisCount>& i_forward)
{
    // 現在位置を検証し、視線を水平単位ベクトルに整える。
    if (!IsFinite(i_position)) { throw std::invalid_argument("基準点は有限値を指定してください。"); }
    const auto forward = Horizontal(i_forward, m_instForward);

    // 前回位置からの移動量をワールド軸ごとに求める。
    const auto dx = static_cast<double>(i_position[defs::kAxisXIndex]) -
                    m_instLastPosition[defs::kAxisXIndex];
    const auto dy = static_cast<double>(i_position[defs::kAxisYIndex]) -
                    m_instLastPosition[defs::kAxisYIndex];
    const auto dz = static_cast<double>(i_position[defs::kAxisZIndex]) -
                    m_instLastPosition[defs::kAxisZIndex];

    // 水平移動量を現在の視線に対する前後・左右の成分に変換する。
    const auto along    =   dx * forward[defs::kHorizontalXIndex] + dz * forward[defs::kHorizontalZIndex];
    const auto lateral  =   dx * -forward[defs::kHorizontalZIndex] + dz * forward[defs::kHorizontalXIndex];

    // 視線の変化を余弦で比較し、設定角度に対応する閾値を求める。
    const auto cosine = std::clamp(
        forward[defs::kHorizontalXIndex] * m_instLastForward[defs::kHorizontalXIndex] +
            forward[defs::kHorizontalZIndex] * m_instLastForward[defs::kHorizontalZIndex],
        defs::kMinimumCosine,
        defs::kMaximumCosine);
    const auto threshold = std::cos(
        m_stSettings.rotationDegrees * defs::kPi / defs::kDegreesPerHalfTurn);

    // いずれかの移動距離または回転角が閾値に達した場合に管理空間を更新する。
    const auto needsUpdate = along >= m_stSettings.forwardThreshold ||
                             along <= -m_stSettings.backwardThreshold ||
                             std::abs(lateral) >= m_stSettings.lateralThreshold ||
                             std::abs(dy) >= m_stSettings.verticalThreshold ||
                             cosine <= threshold;
    if (!needsUpdate)
    {
        // 閾値未満なら原点と比較基準を保ち、現在の視線だけ更新する。
        m_instForward = forward;
        return {m_instOrigin, {}, false};
    }

    // 目標原点との差をセル単位に切り捨て、端数は次回の追従計算に残す。
    const auto desired = DesiredOrigin(m_stSettings, i_position, forward);
    auto offset = std::array<int, defs::kAxisCount>{};
    auto next = m_instOrigin;
    for (auto axis = std::size_t{}; axis < defs::kAxisCount; ++axis)
    {
        offset[axis] = ToCell(std::trunc(desired[axis] - m_instOrigin[axis]));
        next[axis] = ToCell(static_cast<double>(m_instOrigin[axis]) + offset[axis]);
    }

    // 移動後の原点と基準点・視線を次回の比較基準として保存する。
    m_instOrigin = next;
    m_instForward = forward;
    m_instLastForward = forward;
    m_instLastPosition = i_position;
    return {m_instOrigin, offset, true};
}

}   // namespace demo::wind