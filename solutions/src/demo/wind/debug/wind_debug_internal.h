/**
 * @file wind_debug_internal.h
 * @brief 風デバッグ機能の内部状態を共有する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_INTERNAL_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_INTERNAL_H

#include "wind_debug.h"
#include "wind_debug_preview.h"
#include <cstdint>
#include <optional>
#include <string>

namespace demo::wind::debug
{
/**
 * @brief デモで生成した発生器と、編集画面の状態を保持する。
 */
struct WindDebug::Impl
{
    motor::WindMotors motors{};                                     // デバッグ専用枠を持つ発生器管理器
    WindRegionSettings regionSettings{};                            // 管理空間の寸法
    simulation::WindSimulation simulation;                          // カメラに追従する風の管理空間
    WireframeRenderer renderer{};                                   // 発生器の線分描画
    WindArrowRenderer arrowRenderer{};                              // GPU 風速からの矢印描画
    WindDebugPreview preview;                                       // 断面テクスチャと表示範囲
    int cellY = regionSettings.size[defs::kAxisYIndex] / 2;         // セル一覧の Y インデックス
    bool regionVisible = true;                                      // 管理空間の範囲表示
    bool gridVisible = false;                                       // セルの格子表示
    bool arrowsVisible = false;                                     // セルの風向き表示
    std::optional<std::array<int, defs::kAxisCount>> gridOrigin{};  // 保存した格子の原点
    std::vector<std::array<float, defs::kAxisCount>> gridLines{};   // 保存した格子線
    bool volumeOpen = true;                                         // 風データ画面の表示状態
    std::vector<motor::Motor> entries{};                            // 画面から生成した発生器
    std::optional<motor::MotorId> selected{};                       // 編集中の発生器
    motor::Settings draft{};                                        // 編集中または新規作成用の設定
    std::array<float, defs::kAxisCount> baseWind{};                 // 実際に適用中の基本風
    std::array<float, defs::kAxisCount> draftBase{};                // 編集中の基本風
    std::string message{};                                          // 操作結果または入力エラー
    std::uint64_t frame = 0;                                        // 読み戻しのためのフレーム番号
    bool visible = true;                                            // 形状表示を行うか
    bool clickCreation = false;                                     // フィールドのクリックで生成するか

    /**
     * @brief カメラを基準に風の管理空間を準備する。
     * @param i_camera 初期位置と視線を取得するカメラ。
     */
    explicit Impl(const render::Camera& i_camera);

    /**
     * @brief 新しい発生器をカメラの前方へ置けるよう入力値を用意する。
     */
    void Reset(const render::Camera& i_camera);

};

}
#endif
