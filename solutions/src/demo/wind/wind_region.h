/**
 * @file wind_region.h
 * @brief ワールド軸に固定された風の管理空間の追従を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_WIND_REGION_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_WIND_REGION_H

#include "defs/wind_region_constants.h"
#include <array>

namespace demo::wind
{

/** 
 * @brief 初期前方の方向を決める座標系。
 */
enum class CoordinateSystem
{
    RightHanded,    // 右手系
    LeftHanded      // 左手系
};

/** 
 * @brief 管理空間の寸法と追従条件設定。各セルの辺は一メートル。
 */
struct WindRegionSettings
{
    std::array<int, defs::kAxisCount>      size{
        defs::kDefaultSize[defs::kAxisXIndex],
        defs::kDefaultSize[defs::kAxisYIndex],
        defs::kDefaultSize[defs::kAxisZIndex]};                                      // X、Y、Z のセル数
    std::array<float, defs::kAxisCount>    reference{
        defs::kDefaultReference[defs::kAxisXIndex],
        defs::kDefaultReference[defs::kAxisYIndex],
        defs::kDefaultReference[defs::kAxisZIndex]};                                 // 管理空間の基準点（右、上、前に沿った位置）
    CoordinateSystem        coordinateSystem    =   CoordinateSystem::RightHanded;   // 座標系
    float                   forwardThreshold    =   defs::kDefaultForwardThreshold;  // 管理空間を移動する前方閾値
    float                   backwardThreshold   =   defs::kDefaultBackwardThreshold; // 管理空間を移動する後方閾値
    float                   lateralThreshold    =   defs::kDefaultLateralThreshold;  // 管理空間を移動する横方向閾値
    float                   verticalThreshold   =   defs::kDefaultVerticalThreshold; // 管理空間を移動する垂直方向閾値
    float                   rotationDegrees     =   defs::kDefaultRotationDegrees;   // 管理空間を移動する回転角度閾値（度）
};

/**
 * @brief   追従による管理空間の変更。
 * @details 原点はワールド軸に沿った最小位置。移動量は一メートル単位。
 */
struct WindRegionMove
{
    std::array<int, defs::kAxisCount>  origin{};    // 管理空間の現在の原点
    std::array<int, defs::kAxisCount>  offset{};    // 各軸の移動セル数
    bool                updated = false;            // 管理空間が更新されたかどうか
};

/**
 * @brief   風の管理空間
 * @details
 *  管理空間の向きは変わらず、残った端数は目標位置との差として保持する。
 *  視線と位置に合わせ、セル単位で管理空間を移動する。
 */
class WindRegion final
{
public:
    /**
     * @brief 初期設定と追従位置から管理空間を作る。
     * @param i_settings 寸法、座標系、および追従条件。
     * @param i_position 基準点のワールド座標。
     * @param i_forward 視線方向。真上または真下の場合は座標系の初期前方を使う。
     * @throws std::invalid_argument 設定値や入力が不正な場合。
     * @throws std::overflow_error 原点が表現範囲を超えた場合。
     */
    WindRegion(const WindRegionSettings& i_settings,
               const std::array<float, defs::kAxisCount>& i_position,
               const std::array<float, defs::kAxisCount>& i_forward);

    /**
     * @brief 基準点と視線を更新し、必要なら管理空間を移動する。
     * @param i_position 現在の基準点。
     * @param i_forward 現在の視線方向。
     * @return 更新後の原点と各軸の移動セル数。
     * @throws std::invalid_argument 入力が不正な場合。
     * @throws std::overflow_error 移動量が表現範囲を超えた場合。
     */
    WindRegionMove Update(const std::array<float, defs::kAxisCount>& i_position,
                          const std::array<float, defs::kAxisCount>& i_forward);

    /** @brief 管理空間の現在の原点を返す。 */
    const std::array<int, defs::kAxisCount>& Origin() const { return m_instOrigin; }

private:
    WindRegionSettings      m_stSettings;                                    // 管理空間の設定
    std::array<int, defs::kAxisCount>      m_instOrigin{};                   // 管理空間の現在の原点
    std::array<float, defs::kAxisCount>    m_instLastPosition{};             // 前回の基準点のワールド座標
    std::array<float, defs::kHorizontalDirectionCount> m_instForward{};      // 現在の視線方向（水平角、垂直角）
    std::array<float, defs::kHorizontalDirectionCount> m_instLastForward{};  // 前回の視線方向（水平角、垂直角）
};
}
#endif
