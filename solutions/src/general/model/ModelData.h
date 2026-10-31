/**
 * @file ModelData.h
 * @brief FBX から読み込んだ静的モデル全体を定義する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_MODEL_MODEL_DATA_H
#define DYNAMIC_WIND_DEMO_GENERAL_MODEL_MODEL_DATA_H

#include "MaterialData.h"
#include "MeshData.h"
#include "TextureData.h"

#include <vector>

namespace loader
{

/**
 * @brief FBX から読み込んだ静的モデル全体。
 */
struct ModelData
{
    std::vector<MeshData>       meshes;     // メッシュデータの配列
    std::vector<MaterialData>   materials;  // マテリアルデータの配列
    std::vector<TextureData>    textures;   // テクスチャデータの配列
};

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_MODEL_MODEL_DATA_H
