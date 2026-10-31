/**
 * @file TextureData.h
 * @brief RGBA8 形式で保持するテクスチャ画像を定義する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_MODEL_TEXTURE_DATA_H
#define DYNAMIC_WIND_DEMO_GENERAL_MODEL_TEXTURE_DATA_H

#include <cstdint>
#include <string>
#include <vector>

namespace loader
{

/**
 * @brief RGBA8 形式で保持するテクスチャ画像。
 */
struct TextureData
{
    std::string                 name;           // テクスチャ名

    /// @brief FBX からの相対参照名。埋め込み画像の場合は FBX 内の名前。
    std::string                 source_path;    // FBX からの相対参照名。埋め込み画像の場合は FBX 内の名前。
    std::uint32_t               width = 0;      // 幅
    std::uint32_t               height = 0;     // 高さ
    std::vector<std::uint8_t>   rgba8;          // RGBA8 形式のピクセルデータ
};

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_MODEL_TEXTURE_DATA_H
