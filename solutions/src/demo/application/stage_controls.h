/**
 * @file stage_controls.h
 * @brief カメラ入力とステージ設定 UI を扱う。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_APPLICATION_STAGE_CONTROLS_H
#define DYNAMIC_WIND_DEMO_DEMO_APPLICATION_STAGE_CONTROLS_H

#include "demo/actor/actor_patrol.h"
#include "general/vec/Vector3.h"
#include "render/camera.h"
#include "render/renderer.h"
#include <string>

struct GLFWwindow;

namespace demo::application
{

/**
 * @brief ImGui が使用していないマウス・キー入力をカメラへ反映する。
 * @param i_window 入力を読み取る GLFW ウィンドウ。
 * @param io_camera 更新するカメラ。
 * @param i_elapsed 前フレームからの経過時間（秒）。
 */
void UpdateCameraInput(GLFWwindow* i_window, render::Camera& io_camera, float i_elapsed);

/**
 * @brief 周回経路とライティングの設定 UI を描画する。
 * @param io_patrol 設定対象の周回状態。nullptr の場合は周回項目を表示しない。
 * @param io_lighting 更新するライティング設定。
 * @param io_direction 編集中の光源方向。
 * @param io_lightError 入力が不正な場合に表示するエラー。
 */
void DrawStageControls(demo::actor::RectanglePatrol* io_patrol, render::Lighting& io_lighting,
                       general::vec::Vector3& io_direction, std::string& io_lightError);

}
#endif
