/**
 * @file beaufort_constansts.h
 * @brief ビューフォート風力階級の分類に使う定数を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEFS_BEAUFORT_CONSTANSTS_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEFS_BEAUFORT_CONSTANSTS_H

#include <array>
#include <string_view>

namespace demo::wind::defs
{

/**
 *  @brief ビューフォート風力階級の閾値（m/s）
 */
inline constexpr auto kBeaufortThresholds = std::array
{
    0.3F,
    1.6F,
    3.4F,
    5.5F,
    8.0F,
    10.8F,
    13.9F,
    17.2F,
    20.8F,
    24.5F,
    28.5F,
    32.7F
}; // 風力 1～12 に上がる風速の下限（m/s）


/**
 *  @brief 風力階級の名称
 */
inline constexpr auto kBeaufortNames      = std::array<std::string_view, 13>
{
    "静穏",
    "至軽風",
    "軽風",
    "軟風",
    "和風",
    "疾風",
    "雄風",
    "強風",
    "疾強風",
    "大強風",
    "全強風",
    "暴風",
    "颶風"
}; // 風力 0～12 の名称
}
#endif