/**
 * @file field_pick.h
 * @brief 画面の位置からフィールドの三角形への命中を求める。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_FIELD_PICK_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_DEBUG_FIELD_PICK_H

#include "../../../general/math/matrix4.h"
#include "../../../general/model/ModelData.h"
#include "../../../render/camera.h"
#include "../defs/wind_region_constants.h"
#include <array>
#include <optional>
#include <vector>

namespace demo::wind::debug
{

/**
 * @brief カメラから描画範囲内へ向かう半直線。
 */
struct PickRay
{
    std::array<double, defs::kAxisCount> origin{};    // ワールド座標の開始位置
    std::array<double, defs::kAxisCount> direction{}; // 長さ 1 の方向
    double nearDistance{};             // 近クリップ面までの距離
    double farDistance{};              // 遠クリップ面までの距離
};

/**
 * @brief 表面への最も手前の命中。
 */
struct PickHit
{
    std::array<float, defs::kAxisCount> position{}; // ワールド座標の命中位置
    double distance{};              // 半直線の開始位置からの距離
};

/**
 * @brief 画面座標を描画画素の中心から通る半直線へ変換する。
 * @param i_camera 描画に使うカメラ。
 * @param i_mouse ウィンドウ左上を原点とするマウス座標。
 * @param i_windowSize ウィンドウの論理サイズ。
 * @param i_framebufferSize 描画先の画素数。
 * @return ウィンドウ内では半直線、外では空。
 * @throws std::invalid_argument 座標やサイズが不正な場合。
 */
std::optional<PickRay> ScreenPickRay(const render::Camera& i_camera, const std::array<double, 2>& i_mouse,
                                     const std::array<int, 2>& i_windowSize,
                                     const std::array<int, 2>& i_framebufferSize);

/**
 * @brief ノード変換済みの三角形を保持し、描画時のワールド変換で命中を調べる。
 */
class FieldSurface final
{
public:
    /**
     * @brief モデルの三角形とノード変換を取り込む。
     * @param i_data 描画するモデルのデータ。
     * @throws std::invalid_argument 三角形の参照や頂点が不正な場合。
     */
    explicit FieldSurface(const loader::ModelData& i_data);

    /**
     * @brief 表裏を区別せず、描画範囲内で最も手前の三角形を探す。
     * @param i_ray ワールド座標の半直線。
     * @param i_world 描画する Entity のワールド変換。
     * @return 命中した場合は位置と距離。
     * @throws std::invalid_argument 半直線が不正な場合。
     * @throws std::overflow_error 命中位置を表現できない場合。
     */
    std::optional<PickHit> Intersect(const PickRay& i_ray, const general::math::Matrix4& i_world) const;

private:
    using Point = std::array<double, defs::kAxisCount>;
    std::vector<std::array<Point, 3>> m_instTriangles; // ノード変換済み三角形
};

}
#endif
