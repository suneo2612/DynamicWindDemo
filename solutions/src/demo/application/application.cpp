/**
 * @file application.cpp
 * @brief デモアプリケーションの初期化とフレーム処理を実行する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-31
 */
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "application.h"
#include "demo/component/model/model_component.h"
#include "demo/component/model/model_traits.h"
#include "demo/actor/actor_patrol.h"
#include "demo/application/imgui_runtime.h"
#include "demo/application/stage_assets.h"
#include "demo/application/stage_controls.h"
#include "demo/ecs/component_world.h"
#include "general/util/path_util.h"
#include "general/vec/Vector3.h"
#include "render/camera.h"
#include "render/renderer.h"
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
#include "demo/wind/debug/field_pick.h"
#include "demo/wind/debug/wind_debug.h"
#include "demo/wind/defs/beaufort_constants.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
using ModelComponent = demo::component::model::ModelComponent;
using Transform = demo::component::transform::LocalTransform;
using World = demo::ecs::ComponentWorld<Transform, ModelComponent>;
using Clock = std::chrono::steady_clock;
using demo::application::ActorTransform;
using demo::application::DrawStageControls;
using demo::application::FieldTransform;
using demo::application::ImGuiRuntime;
using demo::application::LoadStage;
using demo::application::ModelBounds;
using demo::application::PatrolCenter;
using demo::application::UpdateCameraInput;

/**
 * @brief GLFW の初期化と終了を管理する。
 */
struct GlfwRuntime final
{
    GlfwRuntime()
    {
        glfwSetErrorCallback([](int i_code, const char* i_message)
        {
            std::cerr << "GLFW [" << i_code << "]: " << i_message << '\n';
        });
        if (!glfwInit()) { throw std::runtime_error("GLFW の初期化に失敗しました。"); }
    }
    ~GlfwRuntime() { glfwTerminate(); }
    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;
};

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
/**
 * @brief 遮蔽物を考慮して、フィールド表面に風発生器を生成する。
 * @param i_ray クリック位置を通るワールド空間の半直線。
 * @param i_field 生成対象となるフィールド表面。
 * @param i_fieldWorld フィールドのワールド変換。
 * @param i_actor 遮蔽判定に使う移動物体の表面。指定しない場合は遮蔽判定を行わない。
 * @param i_actorWorld 遮蔽判定に使う移動物体のワールド変換。i_actor と組で指定する。
 * @param io_debug 生成設定と生成処理を管理するデバッグ機能。
 * @return フィールドに命中し、手前に遮蔽物がなく、発生器を生成できた場合は true。
 */
bool CreateOnField(const demo::wind::debug::PickRay& i_ray,
                   const demo::wind::debug::FieldSurface& i_field,
                   const general::math::Matrix4& i_fieldWorld,
                   const demo::wind::debug::FieldSurface* i_actor,
                   const general::math::Matrix4* i_actorWorld,
                   demo::wind::debug::WindDebug& io_debug)
{
    // 半直線上でフィールドに最も近い交点を求め、命中しなければ終了する。
    const auto hit = i_field.Intersect(i_ray, i_fieldWorld);
    if (!hit) { return false; }

    // 移動物体がフィールドより手前に交差する場合は、背後への配置を防ぐ。
    if (i_actor != nullptr && i_actorWorld != nullptr)
    {
        const auto obstruction = i_actor->Intersect(i_ray, *i_actorWorld);
        if (obstruction && obstruction->distance < hit->distance) { return false; }
    }

    // 命中位置に、デバッグ画面で選択中の設定を使って発生器を配置する。
    return io_debug.CreateAt(hit->position);
}
#endif

/**
 * @brief 初期化後の GLFW ウィンドウで描画ループを実行する。
 * @return 終了コード。
 */
int RunInternal()
{
    // GLFW を初期化し、描画資源を作る前に OpenGL コンテキストを持つウィンドウを用意する。
    auto glfw = GlfwRuntime{};
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);                  // OpenGL のメジャーバージョンを 4 にする
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                  // OpenGL のマイナーバージョンを 3 にする
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // コアプロファイルを要求する

    // ウィンドウを RAII で管理し、作成に失敗した場合はここで検出する。
    auto window = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>(
        glfwCreateWindow(1280, 720, "Dynamic Wind - Model Viewer", nullptr, nullptr), glfwDestroyWindow);
    if (!window) { throw std::runtime_error("OpenGL 4.3 ウィンドウを作成できません。"); }

    // 後続の OpenGL 呼び出しがこのウィンドウのコンテキストを使うようにする。
    glfwMakeContextCurrent(window.get());
    // GLEW にコアプロファイルの OpenGL 関数も読み込ませる。
    glewExperimental = GL_TRUE;
    const auto glewResult = glewInit();
    if (glewResult != GLEW_OK)
    {
        throw std::runtime_error("GLEW の初期化に失敗しました: " +
            std::string(reinterpret_cast<const char*>(glewGetErrorString(glewResult))));
    }
    // コアプロファイルで初期化時に残る GL_INVALID_ENUM は許容し、それ以外のエラーを検出する。
    const auto probeError = glGetError();
    if (probeError != GL_NO_ERROR && probeError != GL_INVALID_ENUM)
    {
        throw std::runtime_error("GLEW の初期化中に OpenGL エラーが発生しました。");
    }
    if (!GLEW_VERSION_4_3) { throw std::runtime_error("OpenGL 4.3 以降が必要です。"); }
    glfwSwapInterval(1);    // バッファ交換を垂直同期に合わせる

    // OpenGL コンテキストを使う描画機能と UI を初期化する。
    auto renderer   =   render::Renderer{};
    auto imgui      =   ImGuiRuntime(window.get());

    // フィールドの描画状態を保持する ECS ワールドと Entity を用意する。
    auto world          =   World{};
    const auto request  = world.RequestCreate(Transform{}, ModelComponent{});   
    world.Update(0.0F);     // キューに積んだ Entity 作成要求を処理する
    const auto result = world.TakeResult(request);
    if (result.status != demo::ecs::RequestStatus::Succeeded || !result.entity)
    {
        throw std::runtime_error("フィールド Entity の作成に失敗しました: " + result.message);
    }

    const auto field = *result.entity;
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    // クリック判定で使うフィールド面と移動物体面を保持する。
    auto fieldSurface = std::shared_ptr<const demo::wind::debug::FieldSurface>{};
    auto actorSurface = std::shared_ptr<const demo::wind::debug::FieldSurface>{};
#endif

    auto actorEntity    =   std::optional<demo::ecs::Entity>{};
    auto patrol         =   std::optional<demo::actor::RectanglePatrol>{};
    {
        // ステージと移動物体のモデルを読み込み、モデル境界に合わせて周回経路を設定する。
        const auto stageDirectory   =   general::util::PathFromUtf8(DYNAMIC_WIND_STAGE_DIRECTORY);
        const auto initialStage     =   stageDirectory / "simple" / "source" / "Map 3D Models FBX.fbx";
        const auto loaded           =   LoadStage(initialStage);
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        fieldSurface = loaded.surface;
#endif
        // 読み込んだモデルをフィールド Entity に割り当てる。
        world.ForEach<ModelComponent>([&](const auto& i_entity, auto& io_component)
        {
            if (i_entity == field) { io_component.model = loaded.model; }
        });
        // フィールドを所定の大きさと位置に配置する。
        world.SetTransform(field, FieldTransform());

        // 周回させる移動物体のモデルを読み込む。
        const auto actorPath    =   stageDirectory.parent_path() / "actor" / "cylinder.fbx";
        const auto actor        =   LoadStage(actorPath);
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        actorSurface = actor.surface;
#endif
        // 読み込んだモデルを持つ Entity の作成要求を処理する。
        const auto actorRequest = world.RequestCreate(Transform{}, ModelComponent{actor.model});
        world.Update(0.0F);
        const auto actorResult = world.TakeResult(actorRequest);
        if (actorResult.status != demo::ecs::RequestStatus::Succeeded || !actorResult.entity)
        {
            throw std::runtime_error("移動物体 Entity の作成に失敗しました: " + actorResult.message);
        }

        // Entity を保持し、両モデルの境界から求めた中心を使って周回経路を設定する。
        actorEntity = *actorResult.entity;
        const auto center = PatrolCenter(loaded.bounds, actor.bounds);
        patrol.emplace(
            demo::actor::PatrolSettings{
                {center.x, center.y, center.z},     // 周回経路の中心（X、Y、Z）
                10.0F,                              // X 方向の幅（m）
                10.0F,                              // Z 方向の奥行き（m）
                2.0F                                // 周回速度（m/s）
            }
        );
        world.SetTransform(*actorEntity, ActorTransform(patrol->Current()));
    }

    // カメラと照明を初期化し、デバッグ機能が有効なら風デバッグ機能も用意する。
    auto camera = render::Camera{};
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    auto windDebug = std::unique_ptr<demo::wind::debug::WindDebug>{};
    windDebug = std::make_unique<demo::wind::debug::WindDebug>(camera);

    // 風の経過時間を計算するため、初回更新時刻を記録する。
    auto previousWind = Clock::now();
#endif
    auto lighting   =   render::Lighting{};
    auto direction  =   general::vec::Vector3(
        lighting.direction.x,
        lighting.direction.y,
        lighting.direction.z);                  // UI で編集する初期の照明方向
    auto lightError =   std::string{};          // 不正な照明設定の説明を UI に表示する
    auto previous   =   Clock::now();           // フレーム間の経過時間を測る基準時刻
    while (!glfwWindowShouldClose(window.get()))
    {
        // 処理停止後に周回物体が大きく飛ばないよう、ECS と周回更新へ渡す経過時間を 0.1 秒までに抑える。
        const auto now      =   Clock::now();
        const auto elapsed  =   std::min(std::chrono::duration<float>(now - previous).count(), 0.1F);
        previous = now;
        glfwPollEvents();
        if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
        }

        // 最小化などで描画領域がない間は、短時間イベントを待って次のフレームを試す。
        auto width  = 0;
        auto height = 0;
        glfwGetFramebufferSize(window.get(), &width, &height);
        if (width <= 0 || height <= 0)
        {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        // GLFW/OpenGL の ImGui バックエンドを更新し、新しい UI フレームを開始する。
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        const auto& io = ImGui::GetIO();

        // UI が使用していない入力をカメラへ反映し、ステージと照明の設定 UI を描画する。
        UpdateCameraInput(window.get(), camera, elapsed);
        DrawStageControls(patrol ? &*patrol : nullptr, lighting, direction, lightError);
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        if (windDebug)
        {
            // 風発生器の設定を操作する UI を描画する。
            windDebug->Controls(camera);
        }

        // UI 操作中でなく、クリック配置が有効な場合の左クリックだけを生成操作として扱う。
        const auto clickField = windDebug && 
                                windDebug->ClickCreationEnabled() &&
                                !io.WantCaptureMouse &&
                                !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
                                ImGui::IsMouseClicked(ImGuiMouseButton_Left);
#endif

        // 周回中の物体があれば姿勢を進め、後で風デバッグ機能へ位置と進行方向を渡す。
        const auto cameraPosition   =   camera.Position();
        const auto cameraForward    =   camera.Forward();
        auto followPose = demo::actor::PatrolPose{
            {cameraPosition.x, cameraPosition.y, cameraPosition.z},
            {cameraForward.x, cameraForward.y, cameraForward.z}};
        if (actorEntity && patrol)
        {
            followPose = patrol->Advance(elapsed);
            world.SetTransform(*actorEntity, ActorTransform(followPose));
        }
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        if (windDebug)
        {
            const auto windElapsed  =   std::chrono::duration<double>(now - previousWind).count();
            windDebug->Advance(windElapsed, followPose.position, followPose.forward, camera);
            previousWind = now;
        }
#endif

        // ECS の変更を反映してワールド変換を同期し、各モデルを現在のカメラと照明で描画する。
        world.Update(elapsed);
        world.SynchronizeTransforms();
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glClearColor(0.05F, 0.05F, 0.08F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        world.ForEachRead<ModelComponent>([&](const auto& i_entity, const auto& i_component)
        {
            if (i_component.model)
            {
                renderer.Draw(*i_component.model, world.Transforms().GetWorldMatrix(i_entity), camera,
                              static_cast<float>(width) / static_cast<float>(height), lighting);
            }
        });

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        // 左クリック位置から半直線を作り、移動物体に遮られないフィールド面へ発生器を置く。
        if (clickField)
        {
            auto windowWidth = 0;
            auto windowHeight = 0;

            // マウス座標と同じ論理単位のウィンドウサイズを取得する。
            glfwGetWindowSize(window.get(), &windowWidth, &windowHeight);
            const auto mouse = std::array<double, 2>{io.MousePos.x, io.MousePos.y};
            // ウィンドウ座標と描画画素数から、クリック位置を通るワールド空間の半直線を求める。
            const auto ray = demo::wind::debug::ScreenPickRay(camera, mouse, {windowWidth, windowHeight}, {width, height});
            if (ray && fieldSurface)
            {
                auto actorWorld = std::optional<general::math::Matrix4>{};
                if (actorEntity)
                {
                    // 手前の物体による遮蔽判定に使うワールド変換を取得する。
                    actorWorld = world.Transforms().GetWorldMatrix(*actorEntity);
                }

                // フィールドと移動物体の交点を比較し、遮られていない位置に発生器を配置する。
                CreateOnField(*ray,
                              *fieldSurface,
                              world.Transforms().GetWorldMatrix(field),
                              actorSurface.get(),
                              actorWorld ? &*actorWorld : nullptr,
                              *windDebug);
            }
        }
#endif

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
        // モデルの描画後に風デバッグ用の表示を描画する。
        if (windDebug)
        {
            windDebug->Draw(camera, static_cast<float>(width) / static_cast<float>(height));
        }
#endif

        // UI を描画してからバッファを交換し、完成したフレームを表示する。
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window.get());
    }
    return 0;
}
}

namespace demo::application
{

int Run()
{
    return RunInternal();
}

}
