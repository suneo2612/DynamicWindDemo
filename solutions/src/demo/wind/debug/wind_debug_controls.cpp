/**
 * @file wind_debug_controls.cpp
 * @brief 風デバッグ操作画面を表示する。
 * @details 発生器と基本風の編集、処理負荷の表示、風管理空間の断面画像とセル値の確認を担当する。
 * 描画やシミュレーションの実処理は各機能のクラスへ委譲し、このファイルでは ImGui の入力と表示を扱う。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "wind_debug_internal.h"
#include "../beaufort.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace demo::wind::debug
{
/**
 * @brief 風デバッグ用の各操作画面を表示する。
 * @param i_camera 新規発生器の初期位置を決めるためのカメラ。
 * @details 発生器、処理負荷、風管理空間の画面をそれぞれの表示関数に委譲する。
 */
void WindDebug::Controls(const render::Camera& i_camera)
{
    ShowMotorControls(i_camera);
    ShowPerformanceControls();
    ShowVolumeControls();
}

/**
 * @brief 発生器と基本風を編集する操作画面を表示する。
 * @param i_camera 新規発生器を初期化する位置と向きの取得元。
 * @details 編集中の値はドラフトとして保持し、適用操作時にのみ発生器へ反映する。
 * 入力不正や生成・更新結果は画面内のメッセージとして表示する。
 */
void WindDebug::ShowMotorControls(const render::Camera& i_camera)
{
    auto& impl  =   *m_instImpl;

    // ビューポートを基準に初回表示位置とサイズを決める。
    // FirstUseEver により、ユーザーが移動・サイズ変更した後の状態は ImGui に保持させる。
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.24F,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.02F),
                            ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.25F, viewport->WorkSize.y * 0.9F),
                             ImGuiCond_FirstUseEver);

    // 表示切り替えは描画側が参照する状態へ直接反映する。
    // 基本風の入力欄は draftBase のみを編集し、適用ボタンを押すまではシミュレーションを変更しない。
    ImGui::Begin("風のデバッグ");
    ImGui::Checkbox("発生器の形状を表示", &impl.visible);
    ImGui::Checkbox("フィールドのクリックで生成", &impl.clickCreation);
    ImGui::Checkbox("風管理空間の範囲を表示", &impl.regionVisible);
    ImGui::Checkbox("セルの格子を表示", &impl.gridVisible);
    ImGui::Checkbox("セルの風向きを表示", &impl.arrowsVisible);
    ImGui::InputFloat3("基本風 (m/s)", impl.draftBase.data());

    // 基本風の大きさを三次元ベクトルのノルムで求め、有限値かつ上限以下の場合だけ確定する。
    // 不正値では現在適用中の baseWind を変更せず、原因を画面へ残す。
    if (ImGui::Button("基本風を適用"))
    {
        const auto speed = std::hypot(static_cast<double>(impl.draftBase[defs::kAxisXIndex]),
                                      static_cast<double>(impl.draftBase[defs::kAxisYIndex]),
                                      static_cast<double>(impl.draftBase[defs::kAxisZIndex]));
        if (!std::isfinite(speed) || speed > 64.0)
        {
            impl.message = "基本風は有限値で、風速を 64 m/s 以下にしてください。";
        }
        else
        {
            impl.baseWind = impl.draftBase;
            impl.message.clear();
        }
    }

    // 新規作成へ切り替えると、選択を解除してカメラ基準の初期設定を用意する。
    if (ImGui::Button("新しい発生器")) { impl.Reset(i_camera); impl.message.clear(); }

    // 一覧から発生器を選ぶと、その設定を編集用ドラフトへコピーする。
    // 無効な発生器にも識別できる表示を付け、一覧から状態を確認できるようにする。
    for (const auto& entry : impl.entries)
    {
        const auto label = std::string("発生器 ") + std::to_string(entry.id) +
                           (entry.settings.enabled ? "" : " (無効)");
        if (ImGui::Selectable(label.c_str(), impl.selected == entry.id))
        {
            impl.selected = entry.id;
            impl.draft = entry.settings;
            impl.message.clear();
        }
    }

    // 形状や発生方法の選択結果は、列挙値としてドラフトへ保持する。
    // 既存発生器の発生方法は生成時に決まるため、選択中は変更できないよう入力を無効化する。
    auto shape = static_cast<int>(impl.draft.shape);
    if (ImGui::Combo("形状", &shape, "球\0円錐\0円柱\0")) { impl.draft.shape = static_cast<motor::Shape>(shape); }
    auto mode = static_cast<int>(impl.draft.mode);
    ImGui::BeginDisabled(impl.selected.has_value());
    if (ImGui::Combo("発生方法", &mode, "継続\0瞬間\0")) { impl.draft.mode = static_cast<motor::Mode>(mode); }
    ImGui::EndDisabled();

    // 各入力欄はドラフトを更新するだけで、適用・生成操作までシミュレーションには反映しない。
    // 球には長さの設定がないため、球以外の形状に限って長さを編集可能にする。
    ImGui::InputFloat3("位置 (m)", impl.draft.position.data());
    ImGui::InputFloat3("方向", impl.draft.direction.data());
    ImGui::InputFloat("風速 (m/s)", &impl.draft.speed);
    ImGui::InputFloat("直径 (m)", &impl.draft.diameter);
    if (impl.draft.shape != motor::Shape::Sphere) { ImGui::InputFloat("長さ (m)", &impl.draft.length); }
    ImGui::Checkbox("距離に応じて減衰", &impl.draft.distanceFalloff);
    ImGui::Checkbox("有効", &impl.draft.enabled);

    // 有効な風速にはビューフォート風力階級を併記する。
    // 負値や非有限値は階級計算へ渡さず、入力条件を案内する。
    if (std::isfinite(impl.draft.speed) && impl.draft.speed >= 0.0F)
    {
        const auto scale = ClassifyBeaufort(impl.draft.speed);
        ImGui::Text("風力 %d: %.*s", scale.level, static_cast<int>(scale.name.size()), scale.name.data());
    }
    else { ImGui::TextUnformatted("風速は 0 以上の有限値にしてください。"); }

    // 選択中なら既存設定の更新、それ以外なら新しい発生器の生成を行う。
    // API が返す補正後の設定をドラフトへ戻し、画面と実際の状態を一致させる。
    if (ImGui::Button(impl.selected ? "設定を適用" : "発生器を生成"))
    {
        try
        {
            // 描画不能な座標を先に拒否し、生成後に表示だけが失敗することを避ける。
            if (impl.selected)
            {
                const auto result = Set(*impl.selected, impl.draft);
                impl.message = result.message;
                if (result.removed)
                {
                    // 更新によって発生器が削除された場合は編集対象も新規作成状態へ戻す。
                    impl.Reset(i_camera);
                }
                else { impl.draft = result.settings; }
            }
            else
            {
                const auto result = Create(impl.draft);
                impl.message = result.message;
                if (result.status == motor::RequestStatus::Succeeded && result.id && result.settings)
                {
                    // 生成成功後は新しい発生器を選択し、適用済み設定を次の編集値にする。
                    impl.selected = *result.id;
                    impl.draft = *result.settings;
                }
                else if (impl.message.empty()) { impl.message = "発生器を生成できませんでした。"; }
            }
        }
        catch (const std::invalid_argument& i_error) { impl.message = i_error.what(); }
        catch (const std::overflow_error& i_error) { impl.message = i_error.what(); }
    }

    // 既存発生器を編集中に限り削除操作を表示し、削除後は新規作成状態へ戻す。
    if (impl.selected)
    {
        ImGui::SameLine();
        if (ImGui::Button("削除"))
        {
            Remove(*impl.selected);
            impl.Reset(i_camera);
            impl.message.clear();
        }
    }

    // 補正理由、入力エラー、生成・更新結果をウィンドウ内で折り返して表示する。
    if (!impl.message.empty()) { ImGui::TextWrapped("%s", impl.message.c_str()); }
    ImGui::End();
}

/**
 * @brief 風シミュレーションの直近の処理負荷を表示する。
 * @details CPU の直近更新時間と、非同期計測で完了した GPU の更新・発生器計算時間を表示する。
 * まだ値が得られていない計測は、0 ms と誤認させないよう計測待ちと表示する。
 */
void WindDebug::ShowPerformanceControls()
{
    auto& impl = *m_instImpl;

    // 処理負荷ウィンドウはビューポート左下に初期配置し、画面の一部だけを使う。
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.01F,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.70F),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.22F, viewport->WorkSize.y * 0.20F),
                             ImGuiCond_FirstUseEver);
    if (ImGui::Begin("風の処理負荷"))
    {
        // 描画中に計測値を一度取得し、各項目を同じ計測結果から表示する。
        const auto performance = impl.simulation.Performance();
        ImGui::TextUnformatted("CPUは直近のAdvance、GPUは非同期の直近完了値");

        // 計測が利用可能な項目だけ実測時間を表示し、未完了・未取得なら待機中とする。
        if (performance.hasCpuUpdate)
        {
            ImGui::Text("CPU 風更新: %.3f ms", performance.cpuUpdateMilliseconds);
        }
        else { ImGui::TextUnformatted("CPU 風更新: 計測待ち"); }
        if (performance.hasGpuStep)
        {
            ImGui::Text("GPU 風更新: %.3f ms", performance.gpuStepMilliseconds);
        }
        else { ImGui::TextUnformatted("GPU 風更新: 計測待ち"); }
        if (performance.hasGpuMotor)
        {
            ImGui::Text("GPU 発生器計算: %.3f ms", performance.gpuMotorMilliseconds);
        }
        else { ImGui::TextUnformatted("GPU 発生器計算: 計測待ち"); }

        // 発生器寄与の計算が今回再構築されたか、キャッシュを再利用したかを示す。
        ImGui::Text("発生器寄与: %s", performance.motorCacheRebuilt ? "再計算" : "キャッシュ");
    }
    ImGui::End();
}

/**
 * @brief 風管理空間の状態、断面画像、およびセルごとの風速を表示する。
 * @details 断面画像はホイールでカーソル位置を基準に拡大縮小し、左ドラッグで移動できる。
 * 下部の表は選択した Y 層のセルを列挙し、各セル中心で取得した XYZ 風速を表示する。
 * @note 管理空間の各寸法と断面テクスチャの数は正であることを前提とする。
 */
void WindDebug::ShowVolumeControls()
{
    auto& impl = *m_instImpl;

    // 管理空間ウィンドウをビューポート右側へ初期配置する。
    // volumeOpen を ImGui に渡すことで、ウィンドウの閉じる操作を状態に反映する。
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.50F,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.02F),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.49F, viewport->WorkSize.y * 0.9F),
                             ImGuiCond_FirstUseEver);
    if (ImGui::Begin("風管理空間", &impl.volumeOpen))
    {
        // シミュレーションの現在原点と設定寸法を並べ、管理空間のワールド範囲を確認できるようにする。
        const auto origin = impl.simulation.Origin();
        const auto& size = impl.regionSettings.size;
        ImGui::Text("原点: (%d, %d, %d)  サイズ: %d x %d x %d",
                    origin[defs::kAxisXIndex], origin[defs::kAxisYIndex], origin[defs::kAxisZIndex],
                    size[defs::kAxisXIndex], size[defs::kAxisYIndex], size[defs::kAxisZIndex]);

        // 管理空間の中心を標本位置として読み戻し状態を確認し、フレーム番号または状態を表示する。
        const auto centerSample = impl.simulation.Sample({
            static_cast<double>(origin[defs::kAxisXIndex]) + static_cast<double>(size[defs::kAxisXIndex]) * 0.5,
            static_cast<double>(origin[defs::kAxisYIndex]) + static_cast<double>(size[defs::kAxisYIndex]) * 0.5,
            static_cast<double>(origin[defs::kAxisZIndex]) + static_cast<double>(size[defs::kAxisZIndex]) * 0.5});
        if (centerSample.status == simulation::WindSampleStatus::Ready)
        {
            ImGui::Text("読み戻しフレーム: %llu", static_cast<unsigned long long>(centerSample.frame));
        }
        else if (centerSample.status == simulation::WindSampleStatus::Preparing)
        {
            ImGui::TextUnformatted("風データを読み込み中です。");
        }
        else { ImGui::TextUnformatted("風データが読み戻し範囲外です。"); }

        // 断面ごとの Y 層を表で選ぶ。ID を列ごとに分け、同名スライダーの状態が混ざらないようにする。
        if (ImGui::BeginTable("風テクスチャ断面", static_cast<int>(impl.preview.previewTextures.size()),
                              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
        {
            for (auto index = std::size_t{}; index < impl.preview.previewTextures.size(); ++index)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(index));
                ImGui::Text("断面 %d", static_cast<int>(index + 1));
                ImGui::SliderInt("Y インデックス", &impl.preview.previewY[index], 0, size[defs::kAxisYIndex] - 1);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // 選択中の Y 層から最新の読み戻し風速を取得し、断面テクスチャを更新する。
        impl.preview.UpdatePreviews();

        // 各断面の画像をテクスチャの縦横比に合わせて並べる。
        // 画像領域を不可視ボタンとして確保し、その項目のホバー・ドラッグ入力を利用する。
        if (ImGui::BeginTable("風テクスチャ画像", static_cast<int>(impl.preview.previewTextures.size()),
                              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
        {
            for (std::size_t index = 0; index < impl.preview.previewTextures.size(); ++index)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(index));

                // X 方向を画像の幅、Z 方向を高さとして、列幅内に収まる表示寸法を決める。
                const auto imageWidth = std::max(1.0F, std::min(160.0F, ImGui::GetContentRegionAvail().x));
                const auto imageHeight = imageWidth * static_cast<float>(size[defs::kAxisZIndex]) /
                                         static_cast<float>(size[defs::kAxisXIndex]);
                const auto imageSize = ImVec2(imageWidth, imageHeight);
                ImGui::InvisibleButton("断面画像", imageSize);

                // ホイール入力をこの画像が受け取れるようにし、後続の拡大操作に使う領域座標を取得する。
                ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
                const auto imageMin = ImGui::GetItemRectMin();
                const auto imageMax = ImGui::GetItemRectMax();
                auto& view = impl.preview.previewViews[index];
                const auto& io = ImGui::GetIO();

                // ホイール位置を画像内の 0～1 座標に変換し、その位置を基準にズームする。
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.0F)
                {
                    view.SetZoom(view.zoom * std::pow(1.25F, io.MouseWheel),
                                 {(io.MousePos.x - imageMin.x) / imageWidth,
                                  (io.MousePos.y - imageMin.y) / imageHeight});
                }

                // ドラッグ量を画像寸法で正規化し、表示範囲を画像上の割合として移動する。
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    view.Pan({io.MouseDelta.x / imageWidth, io.MouseDelta.y / imageHeight});
                }

                // 断面テクスチャのうち、現在のズーム・移動範囲に対応する部分を描画する。
                ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(impl.preview.previewTextures[index]),
                                                     imageMin, imageMax, view.Min(), view.Max());

                // スライダーによるズームは画像中央を基準にする。ホイール操作のズーム値もここへ反映する。
                auto zoom = view.zoom;
                ImGui::SetNextItemWidth(imageWidth);
                if (ImGui::SliderFloat("##ズーム", &zoom, 1.0F, 16.0F, "%.1fx"))
                {
                    view.SetZoom(zoom, {0.5F, 0.5F});
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // 断面画像の操作方法と、色の各成分が表す風速範囲を説明する。
        ImGui::TextUnformatted("画像上のホイールで拡大・縮小、左ドラッグで移動できます。");
        ImGui::TextUnformatted("RGB は風速 XYZ 成分です。-32.7 ～ 32.7 m/s を 0 ～ 255 に対応させます。");

        ImGui::Separator();

        // セル一覧に表示する Y 層を選択する。入力範囲は管理空間内の Y インデックス。
        ImGui::SliderInt("セル表示 Y インデックス", &impl.cellY, 0, size[defs::kAxisYIndex] - 1);

        // 行数が多い場合に全行を毎フレーム描画しないよう、表示範囲だけを生成する表を作る。
        if (ImGui::BeginTable("風セル一覧", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0.0F, 300.0F)))
        {
            ImGui::TableSetupColumn("X");
            ImGui::TableSetupColumn("Z");
            ImGui::TableSetupColumn("風速 X");
            ImGui::TableSetupColumn("風速 Y");
            ImGui::TableSetupColumn("風速 Z");
            ImGui::TableHeadersRow();

            // X が最速で変化する行番号から、選択した Y 層内の X・Z セル位置を復元する。
            const auto count = size[defs::kAxisXIndex] * size[defs::kAxisZIndex];
            auto clipper = ImGuiListClipper{};
            clipper.Begin(count);
            while (clipper.Step())
            {
                for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const auto x = row % size[defs::kAxisXIndex];
                    const auto z = row / size[defs::kAxisXIndex];

                    // セル中央のワールド座標で風速を標本化する。原点を加えて管理空間の移動も反映する。
                    const auto sample = impl.simulation.Sample({
                        static_cast<double>(origin[defs::kAxisXIndex]) + static_cast<double>(x) + 0.5,
                        static_cast<double>(origin[defs::kAxisYIndex]) + static_cast<double>(impl.cellY) + 0.5,
                        static_cast<double>(origin[defs::kAxisZIndex]) + static_cast<double>(z) + 0.5});

                    // セルのローカル X・Z 番号と、標本結果の XYZ 成分を各列へ表示する。
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", x);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", z);
                    for (auto axis = std::size_t{}; axis < sample.wind.size(); ++axis)
                    {
                        ImGui::TableSetColumnIndex(static_cast<int>(axis + 2));
                        ImGui::Text("%.3f", sample.wind[axis]);
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    // Begin の戻り値によらず End を呼び、ImGui ウィンドウの開始・終了を対応させる。
    ImGui::End();
}

}
