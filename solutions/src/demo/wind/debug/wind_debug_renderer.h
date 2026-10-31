/**
 * @file wind_debug_renderer.h
 * @brief 風の線分と矢印を描画する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_RENDERER_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_RENDERER_H

#include "../defs/wind_region_constants.h"
#include "../../../render/camera.h"
#include <array>
#include <memory>
#include <vector>

namespace demo::wind::debug
{

/**
 * @brief ワールド座標の線分を深度付きで描画する。
 * @details 全操作と破棄には作成時と同じ有効な OpenGL コンテキストが必要。
 */
class WireframeRenderer final
{
public:
    /**
     * @brief 描画用の GPU 資源を作る。
     * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
     */
    WireframeRenderer();
    ~WireframeRenderer();
    WireframeRenderer(const WireframeRenderer&) = delete;
    WireframeRenderer& operator=(const WireframeRenderer&) = delete;

    /**
     * @brief 線分を描画する。
     * @param i_vertices 2 頂点ずつの線分。
     * @param i_camera 描画に使うカメラ。
     * @param i_aspect 画面の幅と高さの比率。
     * @param i_color 線の RGB 色。
     * @throws std::invalid_argument 頂点や色が不正な場合。
     * @throws std::runtime_error GPU 処理に失敗した場合。
     */
    void Draw(const std::vector<std::array<float, defs::kAxisCount>>& i_vertices, const render::Camera& i_camera,
              float i_aspect, const std::array<float, 3>& i_color) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_instImpl; // 描画資源の所有権
};

/**
 * @brief GPU の風速テクスチャから各セルの風向きを線の矢印として描画する。
 * @details 全操作と破棄には作成時と同じ有効な OpenGL 4.3 コンテキストが必要。
 */
class WindArrowRenderer final
{
public:
    /**
     * @brief 描画用の GPU 資源を作る。
     * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
     */
    WindArrowRenderer();
    ~WindArrowRenderer();
    WindArrowRenderer(const WindArrowRenderer&) = delete;
    WindArrowRenderer& operator=(const WindArrowRenderer&) = delete;

    /**
     * @brief 現在の GPU 風速を使い、全セルに矢印をインスタンシング描画する。
     * @param i_textures XYZ 成分順の符号付き 16.16 固定小数点 R32I 3D テクスチャ。
     * @param i_origin 管理空間の最小 XYZ 座標。
     * @param i_size XYZ 各軸のセル数。テクスチャの寸法と一致すること。
     * @param i_camera 描画に使うカメラ。
     * @param i_aspect 画面の幅と高さの比率。
     * @throws std::invalid_argument テクスチャ名、寸法または画面比率が不正な場合。
     * @throws std::runtime_error GPU 処理に失敗した場合。
     */
    void Draw(const std::array<unsigned int, defs::kAxisCount>& i_textures,
              const std::array<int, defs::kAxisCount>& i_origin,
              const std::array<int, defs::kAxisCount>& i_size, const render::Camera& i_camera, float i_aspect) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_instImpl;
};

}
#endif
