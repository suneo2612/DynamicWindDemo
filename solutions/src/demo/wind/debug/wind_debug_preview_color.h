/**
 * @file wind_debug_preview_color.h
 * @brief 風速をプレビュー色に変換する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_PREVIEW_COLOR_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_PREVIEW_COLOR_H

#include "../defs/wind_region_constants.h"
#include <array>

namespace demo::wind::debug
{

/**
 * @brief 風速 XYZ 成分を符号付きの RGB プレビュー色に変換する。
 * @param i_wind 風速の XYZ 成分（m/s）。
 * @return RGBA8 色。各成分は -32.7、0、32.7 m/s をそれぞれ 0、128、255 に対応させる。
 * @details 範囲を超える成分は端の色に制限する。アルファ値は 255。
 * @throws std::invalid_argument 風速に有限でない値が含まれる場合。
 */
std::array<unsigned char, 4> EncodeWindPreviewColor(const std::array<float, defs::kAxisCount>& i_wind);

}
#endif
