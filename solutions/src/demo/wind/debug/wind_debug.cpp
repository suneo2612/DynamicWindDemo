/**
 * @file wind_debug.cpp
 * @brief 風発生器の操作とシミュレーションを統合する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "wind_debug_internal.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace demo::wind::debug
{
namespace
{

/**
 * @brief カメラのベクトルを風の API に渡す配列に変換する。
 * @param i_point 変換する三次元ベクトル。
 * @return X、Y、Z 成分を順に格納した配列。
 */
std::array<float, defs::kAxisCount> CameraPoint(const general::vec::Vector3& i_point)
{
    return {i_point.x, i_point.y, i_point.z};
}

/**
 * @brief 発生器の寸法補正を考慮して、表示可能な形状か確認する。
 * @param i_settings 確認する発生器の設定。
 * @throws std::invalid_argument 形状、方向、位置または補正後の寸法を描画できない場合。
 * @details 寸法を一時的に補正して線分を生成する。入力設定は変更しない。
 */
void ValidateDisplay(const motor::Settings& i_settings)
{
    auto preview        =   i_settings;
    preview.diameter    =   std::max(preview.diameter, 1.0F);   // 風発生器には1m以上の直径が必要

    // 風発生器の形状が球以外の場合は長さも1m以上に補正する
    if (preview.shape != motor::Shape::Sphere) { preview.length = std::max(preview.length, 1.0F); }
    static_cast<void>(BuildWireframe(preview));
}
}

/**
 * @brief カメラを基準に風の管理空間と表示状態を準備する。
 * @param i_camera 初期位置と視線を取得するカメラ。
 */
WindDebug::Impl::Impl(const render::Camera& i_camera)
    : simulation(regionSettings, CameraPoint(i_camera.Position()), CameraPoint(i_camera.Forward()), {}),
      preview(regionSettings, simulation)
{
    Reset(i_camera);
}

/**
 * @brief 新しい発生器の入力値を用意する。
 * @param i_camera 初期位置と視線を取得するカメラ。
 */
void WindDebug::Impl::Reset(const render::Camera& i_camera)
{
    selected.reset();
    draft   =   motor::Settings{};

    // カメラの前方に新しい発生器を配置するための位置を計算する
    const auto position =   CameraPoint(i_camera.Position());
    const auto forward  =   CameraPoint(i_camera.Forward());
    for (auto axis = std::size_t{}; axis < position.size(); ++axis)
    {
        draft.position[axis] = position[axis] + forward[axis] * 5.0F;
    }
    draft.diameter = 4.0F;
    draft.length = 4.0F;
    draft.speed = 10.0F;
}

/**
 * @brief カメラを基準に風の管理空間と表示状態を作る。
 * @param i_camera 管理空間と発生器の初期位置に使うカメラ。
 * @pre 有効な OpenGL 4.3 コンテキスト上で呼び出す。
 * @throws std::invalid_argument カメラの位置や視線が不正な場合。
 * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
 */
WindDebug::WindDebug(const render::Camera& i_camera) : m_instImpl(std::make_unique<Impl>(i_camera)) {}

/**
 * @brief 風の管理空間と表示用 GPU 資源を解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
WindDebug::~WindDebug() = default;

/**
 * @brief デバッグ専用枠に新しい発生器を生成する。
 * @param i_settings 発生器の設定。
 * @return 生成結果と、補正された場合の理由。
 * @throws std::invalid_argument 設定や表示座標が不正な場合。
 * @throws std::overflow_error 要求または発生器の識別番号を使い切った場合。
 */
motor::CreateResult WindDebug::Create(const motor::Settings& i_settings)
{
    auto& impl = *m_instImpl;
    ValidateDisplay(i_settings);

    // 新規分を確保
    impl.entries.reserve(impl.entries.size() + 1);

    // デバッグ枠の発生器を生成するリクエストを作成する
    const auto request = impl.motors.RequestCreateDebug(i_settings); 
    impl.motors.ProcessRequests();
    const auto result = impl.motors.TakeResult(request);
    if (result.status == motor::RequestStatus::Succeeded && result.id && result.settings)
    {
        // 成功した場合はエントリに追加する
        impl.entries.push_back({*result.id, *result.settings});
    }
    return result;
}

/**
 * @brief 編集中の設定を使い、指定位置に新しい発生器を生成する。
 * @param i_position フィールド表面のワールド座標。
 * @return 生成に成功した場合は true。
 * @details 失敗や補正の理由を操作画面に表示する。成功時は生成した発生器を選択する。
 */
bool WindDebug::CreateAt(const std::array<float, defs::kAxisCount>& i_position)
{
    auto& impl          =   *m_instImpl;
    auto settings       =   impl.draft;
    settings.position   =   i_position;
    try
    {
        // 編集中の形状や風速はそのまま使い、位置だけをクリック地点に置き換える。
        const auto result   =   Create(settings);
        impl.message        =   result.message;
        if (result.status == motor::RequestStatus::Succeeded && result.id && result.settings)
        {
            impl.selected   =   *result.id;         // 選択状態にする
            impl.draft      =   *result.settings;   // 編集中の設定を更新する
            return true;
        }
        if (impl.message.empty()) { impl.message = "発生器を生成できませんでした。"; }
    }
    catch (const std::invalid_argument& i_error) { impl.message = i_error.what(); }
    catch (const std::overflow_error& i_error) { impl.message = i_error.what(); }
    return false;
}

/**
 * @brief フィールドクリックによる発生器生成が有効か取得する。
 * @return 有効な場合は true。
 */
bool WindDebug::ClickCreationEnabled() const
{
    return m_instImpl->clickCreation;
}

/**
 * @brief デバッグ発生器の設定を変更し、画面に保持した設定を更新する。
 * @param i_id 対象の発生器の識別番号。
 * @param i_settings 新しい設定。発生方法は変更できない。
 * @return 設定結果と、補正された場合の理由や適用値。
 * @throws std::invalid_argument 対象が存在しない、設定や表示座標が不正、または発生方法を変更した場合。
 * @details 設定変更により発生器が削除された場合は一覧と選択状態からも取り除く。
 */
motor::SetResult WindDebug::Set(const motor::MotorId i_id, const motor::Settings& i_settings)
{
    auto& impl          =   *m_instImpl;

    // 指定した ID の風発生器エントリを検索する
    const auto found    =   std::find_if(impl.entries.begin(),
                                         impl.entries.end(),
                                         [&](const auto& i_entry) {
                                            return i_entry.id == i_id;
                                        });
    if (found == impl.entries.end()) { throw std::invalid_argument("デバッグ発生器が見つかりません。"); }
    ValidateDisplay(i_settings);

    // 風発生器の設定を適用
    const auto result = impl.motors.Set(i_id, i_settings);
    if (result.removed)
    {
        impl.entries.erase(found);
        if (impl.selected == i_id) { impl.selected.reset(); }
    }
    else 
    {
        found->settings = result.settings;
    }
    return result;
}

/**
 * @brief デバッグ発生器を削除し、選択中なら選択も解除する。
 * @param i_id 削除する発生器の識別番号。
 * @throws std::invalid_argument 対象が存在しない場合。
 */
void WindDebug::Remove(const motor::MotorId i_id)
{
    auto& impl = *m_instImpl;
    const auto found = std::find_if(
        impl.entries.begin(),
        impl.entries.end(),
        [&](const auto& i_entry) 
        {
            return i_entry.id == i_id;
        });
    if (found == impl.entries.end()) { throw std::invalid_argument("デバッグ発生器が見つかりません。"); }
    impl.motors.Remove(i_id);
    impl.entries.erase(found);
    if (impl.selected == i_id) { impl.selected.reset(); }
}

/**
 * @brief 画面から生成したデバッグ発生器の一覧を取得する。
 * @return 無効な発生器を含む一覧のコピー。
 */
std::vector<motor::Motor> WindDebug::Entries() const
{
    return m_instImpl->entries;
}

/**
 * @brief 指定位置の読み戻し済みの風を取得する。
 * @param i_position ワールド座標。
 * @return 風速、読み戻し状態とフレーム番号。
 * @throws std::invalid_argument 座標が不正な場合。
 */
simulation::WindSample WindDebug::Sample(const std::array<double, defs::kAxisCount>& i_position) const
{
    return m_instImpl->simulation.Sample(i_position);
}

/**
 * @brief 物体の位置と進行方向を基準に風を更新する。
 * @param i_elapsed 前回からの経過秒数。
 * @param i_position 管理空間の追従基準となる物体の位置。
 * @param i_forward 物体の進行方向。
 * @param i_camera 瞬間発生器の反映後、編集設定の初期位置に使うカメラ。
 * @throws std::invalid_argument 経過時間、位置または進行方向などが不正な場合。
 * @throws std::overflow_error フレーム番号、未処理時間または管理空間が表現範囲を超えた場合。
 * @throws std::runtime_error GPU 処理または読み戻しに失敗した場合。
 * @details 風を更新した場合、反映済みの瞬間発生器を画面の一覧からも取り除く。
 */
void WindDebug::Advance(const double i_elapsed,
                        const std::array<float, defs::kAxisCount>& i_position,
                        const std::array<float, defs::kAxisCount>& i_forward,
                        const render::Camera& i_camera)
{
    auto& impl = *m_instImpl;
    if (impl.frame == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::overflow_error("風の表示用フレーム番号を使い切りました。");
    }

    // 風の更新を行い、空間の更新回数を取得する
    const auto count    =   impl.simulation.Advance(i_elapsed, impl.frame + 1, i_position, i_forward, impl.baseWind, impl.motors);
    ++impl.frame;

    // 更新があった場合の処理
    if (count > 0)
    {
        // 一度だけの風は更新で解放されるため、画面に保持した設定も取り除く。
        const auto removed = [&](const auto& i_entry)
        {
            // 瞬間風発生器なら更新後に削除されるため、画面からも取り除く。
            return i_entry.settings.enabled && i_entry.settings.mode == motor::Mode::Instant;
        };
        if (impl.selected && 
            std::any_of(
                impl.entries.begin(),
                impl.entries.end(),
                [&](const auto& i_entry) {
                     return i_entry.id == *impl.selected && removed(i_entry); }
                )
            )
        {
            impl.Reset(i_camera);
        }
        std::erase_if(impl.entries, removed);
    }
}

/**
 * @brief カメラの位置と視線を基準に風を更新する。
 * @param i_elapsed 前回からの経過秒数。
 * @param i_camera 管理空間の追従基準と発生器操作に使うカメラ。
 * @throws std::invalid_argument 経過時間、カメラの位置または視線などが不正な場合。
 * @throws std::overflow_error フレーム番号、未処理時間または管理空間が表現範囲を超えた場合。
 * @throws std::runtime_error GPU 処理または読み戻しに失敗した場合。
 */
void WindDebug::Advance(const double i_elapsed, const render::Camera& i_camera)
{
    Advance(i_elapsed, CameraPoint(i_camera.Position()), CameraPoint(i_camera.Forward()), i_camera);
}

/**
 * @brief 表示が有効な発生器、管理空間、セル格子と風向きを描画する。
 * @param i_camera 描画に使うカメラ。
 * @param i_aspect 画面の幅と高さの比率。
 * @pre 作成時と同じ有効な OpenGL 4.3 コンテキスト上で呼び出す。
 * @throws std::invalid_argument 描画用の座標、寸法または画面比率が不正な場合。
 * @throws std::overflow_error セル格子の座標精度または頂点数が表現範囲を超えた場合。
 * @throws std::runtime_error GPU 描画に失敗した場合。
 * @details セル格子は管理空間の原点が変わった場合に再生成する。
 */
void WindDebug::Draw(const render::Camera& i_camera, const float i_aspect) const
{
    auto& impl          =   *m_instImpl;
    const auto origin   =   impl.simulation.Origin();
    
    // 管理空間の領域を描画
    if (impl.regionVisible)
    {
        impl.renderer.Draw(BuildRegionWireframe(origin, impl.regionSettings.size), i_camera, i_aspect, {0.1F, 0.8F, 1.0F});
    }

    // 風セル空間を描画
    if (impl.gridVisible)
    {
        if (!impl.gridOrigin || *impl.gridOrigin != origin)
        {
            impl.gridLines = BuildCellGridWireframe(origin, impl.regionSettings.size);
            impl.gridOrigin = origin;
        }
        impl.renderer.Draw(impl.gridLines, i_camera, i_aspect, {0.2F, 0.45F, 0.55F});
    }

    // 風向きを描画
    if (impl.arrowsVisible)
    {
        impl.arrowRenderer.Draw(impl.simulation.Textures(), origin, impl.regionSettings.size, i_camera, i_aspect);
    }
    if (!impl.visible) { return; }

    // 各発生器を描画
    for (const auto& entry : impl.entries)
    {
        // 風を発生させるかどうかに応じて色を決定
        const auto color = entry.settings.enabled ? std::array{1.0F, 0.8F, 0.1F}
                                                  : std::array{0.5F, 0.5F, 0.5F};
        impl.renderer.Draw(BuildWireframe(entry.settings), i_camera, i_aspect, color);
    }
}
}
