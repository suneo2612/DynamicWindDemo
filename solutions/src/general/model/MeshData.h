/**
 * @file MeshData.h
 * @brief 1つのノードに属する三角形メッシュを定義する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_MODEL_MESH_DATA_H
#define DYNAMIC_WIND_DEMO_GENERAL_MODEL_MESH_DATA_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace loader
{

/**
 * @brief 1つのノードに属する三角形メッシュ。
 * @details 頂点属性はメッシュインデックス単位で格納し、UV セットは独立して保持する。
 */
struct MeshData
{
    std::string name;                                                      // メッシュ名
    /// @brief ufbx の geometry-to-world を列優先で格納した変換行列。
    std::array<float, 16> node_transform
    {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F
    };      // ノード変換行列
    std::vector<std::array<float, 3>>               positions;                  // 頂点位置
    std::vector<std::array<float, 3>>               normals;                    // 頂点法線
    std::vector<std::vector<std::array<float, 2>>>  uv_sets;                    // UV セット
    std::vector<std::string>                        uv_set_names;               // UV セット名
    std::vector<std::uint32_t>                      indices;                    // インデックスバッファ
    /// @brief 三角形ごとの ModelData::materials のインデックス。
    std::vector<std::uint32_t>                      triangle_material_indices;  // 三角形ごとのマテリアルインデックス
};

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_MODEL_MESH_DATA_H
