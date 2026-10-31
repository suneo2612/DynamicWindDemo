/**
 * @file stage_controls.cpp
 * @brief カメラ入力とステージ設定 UI を実装する。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#include "stage_controls.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <array>
#include <cmath>

namespace demo::application
{

/**
 * @brief ImGui が捕捉していない入力だけをカメラへ反映する。
 * @param i_window キー入力を取得する GLFW ウィンドウ。
 * @param io_camera 入力に応じて回転・移動するカメラ。
 * @param i_elapsed 前フレームからの経過時間（秒）。
 * @details マウス操作中の視点回転と WASD による移動を行い、移動量は経過時間に応じて調整する。
 */
void UpdateCameraInput(GLFWwindow* i_window,
                       render::Camera& io_camera,
                       const float i_elapsed)
{
    // UI 操作中の入力がカメラにも適用されないよう、ImGui の捕捉状態を確認する。
    const auto& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        // ImGui がマウスを捕捉していない場合のみ回転を適用する
        io_camera.Rotate(io.MouseDelta.x, io.MouseDelta.y);
    }
    
    // キーボード入力も ImGui が捕捉していない場合のみ適用する
    // WASD による移動
    if (!io.WantCaptureKeyboard)
    {
        // 反対方向のキー入力を相殺し、フレーム時間に比例した移動量をカメラへ渡す。
        const auto forward = (glfwGetKey(i_window, GLFW_KEY_W) == GLFW_PRESS ? 1.0F : 0.0F) -
            (glfwGetKey(i_window, GLFW_KEY_S) == GLFW_PRESS ? 1.0F : 0.0F);
        const auto right = (glfwGetKey(i_window, GLFW_KEY_D) == GLFW_PRESS ? 1.0F : 0.0F) -
            (glfwGetKey(i_window, GLFW_KEY_A) == GLFW_PRESS ? 1.0F : 0.0F);
        io_camera.Move(forward * i_elapsed * 5.0F, right * i_elapsed * 5.0F);
    }
}

/**
 * @brief 周回経路とライティングを編集するステージ設定画面を描画する。
 * @param io_patrol スライダー操作で設定を変更する周回状態。nullptr の場合は周回項目を省略する。
 * @param io_lighting 有効状態と有効な光源方向を更新する設定。
 * @param io_direction 光源方向の入力値。
 * @param io_lightError 不正な方向が入力された場合の表示文。正常な入力で消去する。
 * @details ウィンドウの初期位置とサイズはメインビューポートを基準に設定する。
 */
void DrawStageControls(demo::actor::RectanglePatrol* io_patrol,
                       render::Lighting& io_lighting,
                       general::vec::Vector3& io_direction,
                       std::string& io_lightError)
{
    // 画面サイズや配置にかかわらず、設定画面をビューポートの左上に初期配置
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.01F,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.02F),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.22F, viewport->WorkSize.y * 0.45F),
                             ImGuiCond_FirstUseEver);

    // ステージ設定ウィンドウの開始
    ImGui::Begin("Stage");

    // モデルに周回設定
    if (io_patrol != nullptr)
    {
        auto dimensions = io_patrol->Dimensions();
        if (ImGui::SliderFloat("巡回幅 (m)", &dimensions[0], 1.0F, 100.0F))
        {
            io_patrol->SetDimensions(dimensions[0], dimensions[1]);
        }
        if (ImGui::SliderFloat("巡回奥行き (m)", &dimensions[1], 1.0F, 100.0F))
        {
            io_patrol->SetDimensions(dimensions[0], dimensions[1]);
        }
        auto speed = io_patrol->Speed();
        if (ImGui::SliderFloat("移動速度 (m/s)", &speed, 0.0F, 20.0F))
        {
            io_patrol->SetSpeed(speed);
        }
    }

    // 影が邪魔な時にライティングを切り替えられるようにする
    ImGui::Checkbox("Lighting", &io_lighting.enabled);

    // ImGui 用の入力値はベクトルへ戻し、有効な場合だけ描画設定へ反映する。
    auto directionInput = std::array{io_direction.x, io_direction.y, io_direction.z};
    if (ImGui::InputFloat3("Light direction", directionInput.data()))
    {
        const auto candidate = general::vec::Vector3(
            directionInput[0], directionInput[1], directionInput[2]);
        io_direction = candidate;
        const auto length = candidate.Length();
        if (std::isfinite(length) && length > 1e-8F)
        {
            io_lighting.direction = candidate;
            io_lightError.clear();
        }
        else { io_lightError = "Light direction must be finite and nonzero."; }
    }
    if (!io_lightError.empty()) { ImGui::TextWrapped("%s", io_lightError.c_str()); }
    ImGui::TextUnformatted("Right drag: look  |  WASD: move");
    ImGui::End();
}

}
