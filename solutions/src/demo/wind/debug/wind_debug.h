/**
 * @file wind_debug.h
 * @brief デモ用の風発生器の操作と形状表示を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_WIND_DEBUG_H

#include "wind_debug_geometry.h"
#include "wind_debug_renderer.h"
#include "wind_debug_preview_color.h"
#include "../defs/wind_region_constants.h"
#include "../motor/wind_motor.h"
#include "../simulation/wind_simulation.h"
#include "../../../render/camera.h"
#include <array>
#include <memory>
#include <vector>

namespace demo::wind::debug
{

/**
 * @brief デモ専用の発生器操作、風の更新と形状表示をまとめる。
 * @details デバッグ専用枠を使う。OpenGL と ImGui の有効なスレッドで利用する。
 */
class WindDebug final
{
public:
    /**
     * @brief カメラを基準に風の管理空間を作る。
     * @param i_camera 管理空間の初期位置と視線。
     */
    explicit WindDebug(const render::Camera& i_camera);
    ~WindDebug();
    WindDebug(const WindDebug&) = delete;
    WindDebug& operator=(const WindDebug&) = delete;

    /**
     * @brief デバッグ専用枠に発生器を生成する。
     * @param i_settings 生成する発生器の設定。
     * @return 生成結果と、補正された場合の理由。
     * @throws std::invalid_argument 設定や表示座標が不正な場合。
     */
    motor::CreateResult Create(const motor::Settings& i_settings);

    /**
     * @brief 編集中の設定を使い、指定位置にデバッグ発生器を生成する。
     * @param i_position フィールド表面のワールド座標。
     * @return 生成に成功した場合は true。失敗や補正の理由は操作画面に表示する。
     */
    bool CreateAt(const std::array<float, defs::kAxisCount>& i_position);

    /**
     * @brief フィールドクリックによる生成が有効か取得する。
     * @return クリック生成が有効なら true。
     */
    bool ClickCreationEnabled() const;

    /**
     * @brief デバッグ発生器の設定を変更する。
     * @param i_id 対象の識別番号。
     * @param i_settings 新しい設定。
     * @return 設定結果と、補正された場合の理由。
     * @throws std::invalid_argument 対象や設定が不正な場合。
     */
    motor::SetResult Set(motor::MotorId i_id, const motor::Settings& i_settings);

    /**
     * @brief デバッグ発生器を削除する。
     * @param i_id 対象の識別番号。
     * @throws std::invalid_argument 対象が存在しない場合。
     */
    void Remove(motor::MotorId i_id);

    /**
     * @brief 無効な発生器を含むデバッグ発生器の一覧を取得する。
     * @return 発生器の設定のコピー。
     */
    std::vector<motor::Motor> Entries() const;

    /**
     * @brief 読み戻し済みの風を指定位置で取得する。
     * @param i_position ワールド座標。
     * @return 風速と読み戻し状態。
     */
    simulation::WindSample Sample(const std::array<double, defs::kAxisCount>& i_position) const;

    /**
     * @brief 発生器の生成、設定、削除を操作する画面を表示する。
     * @param i_camera 新規発生器の初期位置に使うカメラ。
     */
    void Controls(const render::Camera& i_camera);

    /**
     * @brief 経過時間と物体の位置・進行方向に合わせて風を更新する。
     * @param i_elapsed 前回からの経過秒数。
     * @param i_position 基準点として使う物体の位置。
     * @param i_forward 物体の進行方向。
     * @param i_camera 発生器操作の初期値に使うカメラ。
     * @throws std::exception 風の更新に失敗した場合。
     */
    void Advance(double i_elapsed, const std::array<float, defs::kAxisCount>& i_position,
                 const std::array<float, defs::kAxisCount>& i_forward, const render::Camera& i_camera);

    /**
     * @brief カメラを基準に風を更新する。
     * @param i_elapsed 前回からの経過秒数。
     * @param i_camera 基準点と発生器操作に使うカメラ。
     */
    void Advance(double i_elapsed, const render::Camera& i_camera);

    /**
     * @brief 発生器、管理空間、セル格子、風向きのうち表示が有効なものを描画する。
     * @param i_camera 描画に使うカメラ。
     * @param i_aspect 画面の幅と高さの比率。
     */
    void Draw(const render::Camera& i_camera, float i_aspect) const;

private:
    /**
     * @brief 発生器と基本風を編集する画面を表示する。
     * @param i_camera 新規発生器の初期位置に使うカメラ。
     */
    void ShowMotorControls(const render::Camera& i_camera);

    /**
     * @brief 風の処理負荷を表示する。
     */
    void ShowPerformanceControls();

    /**
     * @brief 管理空間と風の断面を表示する。
     */
    void ShowVolumeControls();

    struct Impl;
    std::unique_ptr<Impl> m_instImpl; // デモ専用の風と表示状態
};

}
#endif
