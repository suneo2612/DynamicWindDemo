/**
 * @file wind_debug_preview.h
 * @brief 風の断面プレビューと表示状態を管理する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_PREVIEW_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_PREVIEW_H

#include "wind_debug_preview_color.h"
#include "../simulation/wind_simulation.h"
#include <GL/glew.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstddef>

namespace demo::wind::debug
{
/**
 * @brief 断面画像の拡大率と表示中心をテクスチャ座標で保持する。
 * @details 初期状態では画像全体を表示する。拡大・移動時は表示範囲が画像外に出ないよう制限する。
 */
struct PreviewView
{
    float zoom = 1.0F;                          // 表示範囲の拡大率
    std::array<float, 2> center{0.5F, 0.5F};    // 表示範囲の中心座標（テクスチャ座標系）

    /**
     * @brief 指定位置を保ちながら拡大率を変更する。
     * @param i_zoom 新しい拡大率。
     * @param i_anchor 表示中の画像内の基準位置。各成分は左上が 0、右下が 1。
     * @details 拡大率を 1～16 に制限する。画像端では表示範囲を優先するため、基準位置が移動する場合がある。
     */
    void SetZoom(const float i_zoom, const std::array<float, 2>& i_anchor)
    {
        const auto next = std::clamp(i_zoom, 1.0F, 16.0F);
        const auto difference = 1.0F / zoom - 1.0F / next;
        for (auto axis = std::size_t{}; axis < center.size(); ++axis)
        {
            center[axis] += (std::clamp(i_anchor[axis], 0.0F, 1.0F) - 0.5F) * difference;
        }
        zoom = next;
        Clamp();
    }

    /**
     * @brief ドラッグ距離に応じて表示範囲を移動する。
     * @param i_delta 画像の幅・高さを 1 としたドラッグ距離。
     * @details 表示範囲が画像外に出る場合は端で停止する。
     */
    void Pan(const std::array<float, 2>& i_delta)
    {
        for (auto axis = std::size_t{}; axis < center.size(); ++axis)
        {
            center[axis] -= i_delta[axis] / zoom;
        }
        Clamp();
    }

    /**
     * @brief 表示範囲の左上を求める。
     * @return 左上のテクスチャ座標。
     */
    ImVec2 Min() const
    {
        const auto half = 0.5F / zoom;
        return {center[0] - half, center[1] - half};
    }

    /**
     * @brief 表示範囲の右下を求める。
     * @return 右下のテクスチャ座標。
     */
    ImVec2 Max() const
    {
        const auto half = 0.5F / zoom;
        return {center[0] + half, center[1] + half};
    }

private:
    /**
     * @brief 拡大率に応じて中心を補正し、表示範囲をテクスチャ内に収める。
     */
    void Clamp()
    {
        const auto half = 0.5F / zoom;
        for (auto& value : center) { value = std::clamp(value, half, 1.0F - half); }
    }
};


/**
 * @brief 風の断面テクスチャと各画像の表示状態を所有する。
 * @details 
 * 指定した Y インデックスの XZ 断面を 5 枚保持する。
 * 管理空間の設定とシミュレーションは参照のみ保持し、所有しない。
 * 全操作と破棄には作成時と同じ有効な OpenGL コンテキストが必要。
 */
class WindDebugPreview final
{
public:
    /**
     * @brief 管理空間の寸法に合わせて断面画像を準備する。
     * @param i_regionSettings 管理空間の設定。プレビューより長く生存すること。
     * @param io_simulation 読み戻し済みの風。プレビューより長く生存すること。
     * @pre 管理空間の寸法が正で、シミュレーションの寸法と一致していること。
     * @throws std::runtime_error 断面テクスチャの作成または初期化に失敗した場合。
     * @details Y インデックスを管理空間の下端から上端まで均等に配置する。
     */
    WindDebugPreview(const WindRegionSettings& i_regionSettings, simulation::WindSimulation& io_simulation);

    /**
     * @brief 所有する断面テクスチャを解放する。
     */
    ~WindDebugPreview();
    WindDebugPreview(const WindDebugPreview&) = delete;
    WindDebugPreview& operator=(const WindDebugPreview&) = delete;

    /**
     * @brief 読み戻し済みの風から各断面テクスチャを更新する。
     * @pre 各 Y インデックスが管理空間の範囲内であること。
     * @throws std::invalid_argument 標本座標または表示する風速が不正な場合。
     * @throws std::runtime_error テクスチャの更新に失敗した場合。
     * @details 各セルの中心の風速を取得して色に変換する。読み戻し前や範囲外では代替風を使用する。
     * 更新時に変更したテクスチャの結合状態は復元する。
     */
    void UpdatePreviews();

    std::array<GLuint, 5> previewTextures{};   // 各断面のテクスチャ
    std::array<int, 5> previewY{};             // 各断面の Y 座標
    std::array<PreviewView, 5> previewViews{}; // 各断面の表示範囲

private:
    /**
     * @brief 断面表示用のテクスチャを確保し初期化する。
     * @throws std::runtime_error テクスチャの作成または初期化に失敗した場合。
     * @details 失敗時は作成済みのテクスチャを解放し、変更した結合状態を復元する。
     */
    void CreatePreviewTextures();

    const WindRegionSettings& m_stRegionSettings; // テクスチャの寸法
    simulation::WindSimulation& m_instSimulation; // 風速の読み戻し元
};
}
#endif
