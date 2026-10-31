/**
 * @file stage_assets.h
 * @brief ステージと移動物体のモデルおよび姿勢を扱う。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_APPLICATION_STAGE_ASSETS_H
#define DYNAMIC_WIND_DEMO_DEMO_APPLICATION_STAGE_ASSETS_H

#include "demo/actor/actor_patrol.h"
#include "demo/component/transform/transform_system.h"
#include "general/model/ModelData.h"
#include "general/vec/Vector3.h"
#include <filesystem>
#include <limits>
#include <memory>

namespace render
{
class Model;
}

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
namespace demo::wind::debug
{
class FieldSurface;
}
#endif

namespace demo::application
{

/**
 * @brief モデルのローカル座標における軸平行境界。
 */
struct ModelBounds
{
    general::vec::Vector3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()}; // 各軸の最小座標。
    general::vec::Vector3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()}; // 各軸の最大座標。
};

/**
 * @brief 読み込んだステージモデルと、そのメッシュ境界を保持する。
 */
struct LoadedStage
{
    std::shared_ptr<const render::Model> model; // GPU に登録した描画モデル。
    ModelBounds bounds;                         // ノード変換適用後の境界。
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    std::shared_ptr<const demo::wind::debug::FieldSurface> surface; // 選択判定に使う三角形表面。
#endif
};

/**
 * @brief FBX を読み込み、GPU モデルとノード変換適用後の境界を返す。
 * @param i_path FBX のパス。
 * @return 描画モデルとモデル境界。
 */
LoadedStage LoadStage(const std::filesystem::path& i_path);

/**
 * @brief 地形の床と移動物体の底面を合わせた周回中心を返す。
 * @param i_field 地形モデルの境界。
 * @param i_actor 移動物体モデルの境界。
 * @return 周回中心のワールド座標（X、Y、Z）。
 */
general::vec::Vector3 PatrolCenter(const ModelBounds& i_field, const ModelBounds& i_actor);

/**
 * @brief 地形を原点基準で一様に拡大する姿勢を返す。
 * @return 地形に適用するローカル変換。
 */
demo::component::transform::LocalTransform FieldTransform();

/**
 * @brief 周回姿勢を Entity の位置、向き、縮尺に変換する。
 * @param i_pose 周回物体の位置と進行方向。
 * @return 移動物体に適用するローカル変換。
 */
demo::component::transform::LocalTransform ActorTransform(const demo::actor::PatrolPose& i_pose);

}
#endif
