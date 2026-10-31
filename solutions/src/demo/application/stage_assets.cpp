/**
 * @file stage_assets.cpp
 * @brief ステージと移動物体のモデル読込および姿勢計算を実装する。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#include "stage_assets.h"
#include "general/math/quaternion.h"
#include "general/util/path_util.h"
#include "general/vec/Vector3.h"
#include "loader/loader_factory.h"
#include "render/renderer.h"
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
#include "demo/wind/debug/field_pick.h"
#endif
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace demo::application
{
namespace
{
constexpr float kActorModelScale = 0.5F;    // 円柱 FBX の寸法を調整する縮尺。
constexpr float kFieldModelScale = 4.0F;    // 地形の幅・高さ・奥行きを拡大する縮尺。

/**
 * @brief メッシュ頂点をノード変換し、モデル境界を拡張する。
 * @param i_data 読み込んだモデルデータ。
 * @param io_bounds 更新する境界。
 * @throws std::runtime_error 境界を計算できる頂点がない、または頂点座標が有限でない場合。
 */
void ExtendBounds(const loader::ModelData& i_data, ModelBounds& io_bounds)
{
    auto found = false;
    // 描画位置と境界を一致させるため、頂点ごとにメッシュのノード変換を適用する。
    for (const auto& mesh : i_data.meshes)
    {
        // FBXのノード配置に合わせる変換行列を取得
        const auto& matrix = mesh.node_transform;
        for (const auto& position : mesh.positions)
        {
            const auto transformed = general::vec::Vector3(
                matrix[0] * position[0] + matrix[4] * position[1] + matrix[8] * position[2] + matrix[12],
                matrix[1] * position[0] + matrix[5] * position[1] + matrix[9] * position[2] + matrix[13],
                matrix[2] * position[0] + matrix[6] * position[1] + matrix[10] * position[2] + matrix[14]);

            // 不正な座標を境界へ混ぜず、モデル読込の失敗として呼び出し側へ伝える。
            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) ||
                !std::isfinite(transformed.z))
            {
                throw std::runtime_error("FBX の境界を計算できない頂点があります。");
            }

            // AABB を更新する。
            io_bounds.minimum.x = std::min(io_bounds.minimum.x, transformed.x);
            io_bounds.minimum.y = std::min(io_bounds.minimum.y, transformed.y);
            io_bounds.minimum.z = std::min(io_bounds.minimum.z, transformed.z);
            io_bounds.maximum.x = std::max(io_bounds.maximum.x, transformed.x);
            io_bounds.maximum.y = std::max(io_bounds.maximum.y, transformed.y);
            io_bounds.maximum.z = std::max(io_bounds.maximum.z, transformed.z);
            found = true;
        }
    }
    // 空のメッシュ群では初期値のままの境界を返さない。
    if (!found) { throw std::runtime_error("FBX に境界を計算できる頂点がありません。"); }
}
}

/**
 * @brief FBX を読み込み、フィールド情報とする
 * @param i_path 読み込む FBX のパス。
 * @return GPU モデル、ノード変換適用後の境界、および有効時は選択用表面。
 * @throws std::runtime_error ローダーがない、読込失敗、境界計算失敗、または GPU 登録失敗の場合。
 */
LoadedStage LoadStage(const std::filesystem::path& i_path)
{
    // ローダーへ渡すパスを UTF-8 に統一し、ファイル読込と描画用モデルの作成を行う。
    const auto path =   general::util::PathToUtf8(i_path);
    auto loader     =   loader::LoaderFactory::CreateLoader(path);
    // ローダーが作成できなかった場合は例外を投げる（今のところFBXのみ対応）
    if (!loader) { throw std::runtime_error("対応するモデルローダーがありません: " + path); }

    // モデルデータを読み込む。
    if (!loader->Load(path)) { throw std::runtime_error(loader->GetLastError()); }

    // モデルのAABBを計算する。
    auto bounds =   ModelBounds{};
    ExtendBounds(loader->GetModelData(), bounds);

    // 描画用モデルデータ作成を作成する。
    auto model = std::make_shared<render::Model>(loader->GetModelData());
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    // デバッグでクリック位置判定に使うのにフィールド表面を作成する。
    auto surface = std::make_shared<demo::wind::debug::FieldSurface>(loader->GetModelData());
    return {std::move(model), bounds, std::move(surface)};
#else
    return {std::move(model), bounds};
#endif
}

/**
 * @brief 地形の床と移動物体の底面を合わせた移動キャラクターの周回中心を求める。
 * @param i_field 地形モデルの境界。
 * @param i_actor 移動物体モデルの境界。
 * @return 周回中心のワールド座標。
 * @details XZ 座標は地形境界の中心、Y 座標は地形の床から物体の底面分だけ上に設定する。
 */
general::vec::Vector3 PatrolCenter(const ModelBounds& i_field, const ModelBounds& i_actor)
{
    return general::vec::Vector3(
        (i_field.minimum.x + i_field.maximum.x) * 0.5F * kFieldModelScale,
        i_field.minimum.y * kFieldModelScale - i_actor.minimum.y * kActorModelScale,
        (i_field.minimum.z + i_field.maximum.z) * 0.5F * kFieldModelScale);
}

/**
 * @brief 地形を原点基準で一様に拡大する姿勢を作成する。
 * @return 地形に適用するローカル変換。
 */
demo::component::transform::LocalTransform FieldTransform()
{
    auto transform = demo::component::transform::LocalTransform{};
    transform.scale = general::vec::Vector3(kFieldModelScale, kFieldModelScale, kFieldModelScale);
    return transform;
}

/**
 * @brief 周回姿勢を移動物体の位置、向き、縮尺へ変換する。
 * @param i_pose 周回物体の位置と進行方向。
 * @return 移動物体に適用するローカル変換。
 * @details モデルのローカル -Z 方向が進行方向を向くよう、Y 軸回転を設定する。
 */
demo::component::transform::LocalTransform ActorTransform(const demo::actor::PatrolPose& i_pose)
{
    // 移動物体のローカル変換を作成する。
    auto transform = demo::component::transform::LocalTransform{};
    transform.position = general::vec::Vector3(i_pose.position[0], i_pose.position[1], i_pose.position[2]);
    transform.scale = general::vec::Vector3(kActorModelScale, kActorModelScale, kActorModelScale);

    // 移動物体の向きを前方向として回転を設定する。
    const auto yaw = std::atan2(-i_pose.forward[0], -i_pose.forward[2]);
    transform.rotation = general::math::Quaternion::FromEulerRadians(general::vec::Vector3(0.0F, yaw, 0.0F));
    return transform;
}

}
