/**
 * @file beaufort.h
 * @brief 風速からビューフォート風力階級を求める。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_BEAUFORT_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_BEAUFORT_H

#include <string_view>

namespace demo::wind
{
/**
 * @brief ビューフォート風力階級とその名称。
 */
struct BeaufortScale
{
    int level;              // ビューフォート風力階級（0 から 12）
    std::string_view name;  // 日本語名称
};

/**
 * @brief 風速を風力 0 から 12 に分類する。
 * @param i_speedMetersPerSecond 負でない有限な風速（m/s）。
 * @return 対応する階級と日本語名称。
 * @throws std::invalid_argument 風速が負または有限でない場合。
 * @details 各階級の下限を境界とし、32.7 m/s 以上は風力 12 とする。
 */
BeaufortScale ClassifyBeaufort(float i_speedMetersPerSecond);

}   // namespace demo::wind
#endif
