/**
 * @file imgui_runtime.h
 * @brief ImGui と描画バックエンドの寿命を管理する。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_APPLICATION_IMGUI_RUNTIME_H
#define DYNAMIC_WIND_DEMO_DEMO_APPLICATION_IMGUI_RUNTIME_H

#include <imgui.h>

struct GLFWwindow;

namespace demo::application
{

/**
 * @brief ImGui コンテキストと GLFW/OpenGL バックエンドを管理する。
 */
class ImGuiRuntime final
{
public:
    /**
     * @brief コンテキストと描画バックエンドを初期化する。
     * @param i_window バックエンドに接続する GLFW ウィンドウ。
     * @throws std::runtime_error バックエンドの初期化に失敗した場合。
     */
    explicit ImGuiRuntime(GLFWwindow* i_window);

    /**
     * @brief 描画バックエンドとコンテキストを解放する。
     */
    ~ImGuiRuntime();

    /**
     * @brief コピーコンストラクタを禁止する。
     */
    ImGuiRuntime(const ImGuiRuntime&) = delete;
    ImGuiRuntime& operator=(const ImGuiRuntime&) = delete;

private:
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    ImVector<ImWchar> m_instGlyphRanges; // フォント生成まで文字範囲を保持する
#endif
};

}
#endif
