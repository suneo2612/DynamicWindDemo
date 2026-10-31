/**
 * @file imgui_runtime.cpp
 * @brief ImGui のフォント、コンテキスト、描画バックエンドを初期化する。
 * @details 日本語フォントは利用できる場合にのみ登録し、読み込めない場合は標準フォントを使用する。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#include "imgui_runtime.h"
#include "general/util/path_util.h"
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
#include "demo/wind/defs/beaufort_constants.h"
#endif

namespace demo::application
{

/**
 * @brief ImGui コンテキストと GLFW/OpenGL バックエンドを初期化する。
 * @param i_window バックエンドに接続する GLFW ウィンドウ。
 * @throws std::runtime_error GLFW または OpenGL バックエンドの初期化に失敗した場合。
 * @details Windows の日本語フォントが利用できる場合は読み込み、利用できない場合も標準フォントで続行する。
 *          OpenGL バックエンドまで初期化できた場合、破棄時に両バックエンドとコンテキストを解放する。
 */
ImGuiRuntime::ImGuiRuntime(GLFWwindow* i_window)
{
    IMGUI_CHECKVERSION();

    // ImGui コンテキストを作成する前に、バックエンドの初期化に必要な設定を行う。
    ImGui::CreateContext();
    
    // ウィンドウごとの設定ファイルを生成せず、起動時の状態を一定にする。
    ImGui::GetIO().IniFilename = nullptr;

    // Windows のフォントディレクトリに日本語フォントがあれば、UI の表示に使用する。
    if (const auto* root = std::getenv("WINDIR"))
    {
        // Windows のフォントディレクトリを取得する。
        const auto fontPath = std::filesystem::path(root) / "Fonts" / "meiryo.ttc";
        if (std::filesystem::exists(fontPath))
        {
            const auto font = general::util::PathToUtf8(fontPath);
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
            // デバッグ画面の語彙も表示できるよう、日本語範囲へ必要な漢字を追加する。
            auto builder = ImFontGlyphRangesBuilder{};
            builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
            for (const auto name : demo::wind::defs::kBeaufortNames)
            {
                builder.AddText(name.data(), name.data() + name.size());
            }
            // デバッグ用に追加した漢字を含む範囲を構築する。
            builder.AddText("風のデバッグ発生器形状表示基本風適用新位置方向速度直径長距離減衰有効生成設定削除セル格子フィールドクリック");

            // フォント生成が完了するまで範囲配列を保持できるよう、メンバーに保存する。
            builder.BuildRanges(&m_instGlyphRanges);
            const auto* ranges = m_instGlyphRanges.Data;
#else
            // 日本語フォントを使用しない場合は、標準の日本語範囲を使用する。
            const auto* ranges = ImGui::GetIO().Fonts->GetGlyphRangesJapanese();
#endif
            // 日本語フォントを ImGui に登録し、読み込みに失敗した場合は標準フォントで続行する。
            if (!ImGui::GetIO().Fonts->AddFontFromFileTTF(font.c_str(), 16.0F, nullptr, ranges))
            {
                // フォントは任意機能のため、読み込み失敗時は ImGui の標準フォントで続行する。
                std::cerr << "日本語フォントを読み込めませんでした。\n";
            }
        }
        else { std::cerr << "日本語フォントが見つかりません。標準フォントを使用します。\n"; }
    }
    // スタイルをダークテーマに設定する。
    ImGui::StyleColorsDark();

    // GLFW バックエンドを先に初期化し、後続の OpenGL 初期化失敗時に先行資源を解放する。
    if (!ImGui_ImplGlfw_InitForOpenGL(i_window, true))
    {
        // GLFW バックエンドを開始できなかったため、作成済みのコンテキストを破棄する。
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui の GLFW 初期化に失敗しました。");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 430 core"))
    {
        // 先に初期化した GLFW バックエンドを停止してから、コンテキストを破棄する。
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui の OpenGL 初期化に失敗しました。");
    }
}

/**
 * @brief 描画バックエンドを解放して ImGui コンテキストを破棄する。
 * @details 初期化と逆の順序でバックエンドを停止してから、共有コンテキストを破棄する。
 */
ImGuiRuntime::~ImGuiRuntime()
{
    ImGui_ImplOpenGL3_Shutdown();   // OpenGL バックエンドを停止する。
    ImGui_ImplGlfw_Shutdown();      // GLFW バックエンドを停止する。
    ImGui::DestroyContext();        // ImGui コンテキストを破棄する。
}

}
