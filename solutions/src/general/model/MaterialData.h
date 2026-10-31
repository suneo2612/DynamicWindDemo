/**
 * @file MaterialData.h
 * @brief 描画に必要な基本マテリアル情報を定義する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_MODEL_MATERIAL_DATA_H
#define DYNAMIC_WIND_DEMO_GENERAL_MODEL_MATERIAL_DATA_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace loader
{

/**
 * @brief 描画に必要な基本マテリアル情報。
 */
struct MaterialData
{
    std::string                 name;                                   // マテリアル名
    std::array<float, 4>        base_color{ 1.0F, 1.0F, 1.0F, 1.0F };   // 基本色
    float opacity   =   1.0F;                                           // 不透明度
    float roughness =   1.0F;                                           // 粗さ
    float metallic  =   0.0F;                                           // 金属度
    std::optional<std::uint32_t>    base_color_texture;                 // 基本色テクスチャ
    std::optional<std::uint32_t>    normal_texture;                     // 法線マップ

    /// @brief 空の場合はメッシュの先頭 UV セットを使用する。
    std::string     base_color_uv_set;                                  // 基本色用 UV セット
    std::string     normal_uv_set;                                      // 法線マップ用 UV セット
};

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_MODEL_MATERIAL_DATA_H
