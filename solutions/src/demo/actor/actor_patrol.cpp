/**
 * @file actor_patrol.cpp
 * @brief 四角形経路上の周回位置と進行方向を計算する。
 * @par 最終更新日 2026-10-31
 */
#include "actor_patrol.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace demo::actor
{

namespace
{

/**
 * @brief 中心座標がすべて有限であることを確認する。
 * @param i_center 確認する X、Y、Z 座標。
 * @throws std::invalid_argument 有限でない座標が含まれる場合。
 */
void ValidateCenter(const std::array<float, 3>& i_center)
{
    if (!std::all_of(i_center.begin(),
                     i_center.end(),
                     [](const float i_value) 
                     {
                        return std::isfinite(i_value);
                    }))
    {
        throw std::invalid_argument("周回中心は有限値で指定してください。");
    }
}

/**
 * @brief 周回寸法が正の有限値であることを確認する。
 * @param i_width X 方向の幅（m）。
 * @param i_depth Z 方向の奥行き（m）。
 * @throws std::invalid_argument 寸法が正の有限値でない場合。
 */
void ValidateDimensions(const float i_width, const float i_depth)
{
    if (!std::isfinite(i_width) || 
        i_width <= 0.0F ||
        !std::isfinite(i_depth) ||
        i_depth <= 0.0F)
    {
        throw std::invalid_argument("周回経路の幅と奥行きは正の有限値で指定してください。");
    }
}

/**
 * @brief 周回長方形の全周の長さを求める。
 * @param i_settings 周回寸法を含む設定。
 * @return 周回長さ（m）。
 */
double Perimeter(const PatrolSettings& i_settings)
{
    return 2.0 * (static_cast<double>(i_settings.width) + i_settings.depth);
}

}

/**
 * @brief 周回設定を検証し、始点から周回を開始する。
 * @param i_settings 経路中心、寸法、速度。
 * @throws std::invalid_argument 中心、寸法、速度が不正な場合。
 */
RectanglePatrol::RectanglePatrol(const PatrolSettings& i_settings)
    : m_stSettings(i_settings)
{
    ValidateCenter(m_stSettings.center);
    ValidateDimensions(m_stSettings.width, m_stSettings.depth);
    SetSpeed(m_stSettings.speed);
}

/**
 * @brief 経過時間に応じて周回位置を進める。
 * @param i_elapsed 経過秒数。
 * @return 更新後の位置と進行方向。
 * @throws std::invalid_argument 経過時間が負または有限でない場合。
 * @throws std::overflow_error 距離または位置を表現できない場合。
 */
PatrolPose RectanglePatrol::Advance(const double i_elapsed)
{
    if (!std::isfinite(i_elapsed) || i_elapsed < 0.0)
    {
        throw std::invalid_argument("周回の経過時間は 0 以上の有限値で指定してください。");
    }
    const auto perimeter    =   Perimeter(m_stSettings);                                // 周回長方形の全周の長さ（m）
    const auto distance     =   static_cast<double>(m_stSettings.speed) * i_elapsed;    // 経過時間に応じた移動距離（m）
    if (!std::isfinite(distance)) { throw std::overflow_error("周回距離を表現できません。"); }
    m_fDistance = std::fmod(m_fDistance + std::fmod(distance, perimeter), perimeter);
    return Current();
}

/**
 * @brief 周回中心を変更し、周回位置を新しい始点へ戻す。
 * @param i_center 新しい周回中心（X、Y、Z）。
 * @throws std::invalid_argument 中心に有限でない値が含まれる場合。
 */
void RectanglePatrol::SetCenter(const std::array<float, 3>& i_center)
{
    ValidateCenter(i_center);
    m_stSettings.center = i_center;
    m_fDistance = 0.0;
}

/**
 * @brief 周回寸法を変更し、経路上の周回割合を保つ。
 * @param i_width X 方向の幅（m）。
 * @param i_depth Z 方向の奥行き（m）。
 * @throws std::invalid_argument 寸法が正の有限値でない場合。
 */
void RectanglePatrol::SetDimensions(const float i_width, const float i_depth)
{
    ValidateDimensions(i_width, i_depth);
    const auto phase    =   m_fDistance / Perimeter(m_stSettings);
    m_stSettings.width  =   i_width;
    m_stSettings.depth  =   i_depth;
    m_fDistance         =   phase * Perimeter(m_stSettings);
}

/**
 * @brief 現在の周回距離から位置と進行方向を求める。
 * @return 現在の物体原点の座標と経路の進行方向。
 * @throws std::overflow_error 位置を表現できない場合。
 * @details 周回距離に応じて +X、+Z、-X、-Z の各辺へ割り当てる。
 */
PatrolPose RectanglePatrol::Current() const
{
    // 距離を周回経路上の各辺の長さと比較しやすいよう、計算精度を double にそろえる。
    const auto width        =   static_cast<double>(m_stSettings.width);
    const auto depth        =   static_cast<double>(m_stSettings.depth);
    const auto halfWidth    =   width * 0.5;
    const auto halfDepth    =   depth * 0.5;
    auto distance           =   m_fDistance;

    // 高さは中心の Y 座標を保ち、周回する XZ 座標だけを各辺の計算で設定する。
    auto pose       =   PatrolPose{};
    pose.position   =   m_stSettings.center;

    // 距離を辺の長さずつ差し引き、該当する辺上の位置と進行方向を求める。
    // 頂点に一致した場合は、次の辺の始点として扱う。
    if (distance < width)
    {
        // 始点から +X 方向へ進む辺。
        pose.position[0]    =   static_cast<float>(static_cast<double>(m_stSettings.center[0]) - halfWidth + distance);
        pose.position[2]    =   static_cast<float>(static_cast<double>(m_stSettings.center[2]) - halfDepth);
        pose.forward = {1.0F, 0.0F, 0.0F};
    }
    else if ((distance -= width) < depth)
    {
        // 右端から +Z 方向へ進む辺。
        pose.position[0]    =   static_cast<float>(static_cast<double>(m_stSettings.center[0]) + halfWidth);
        pose.position[2]    =   static_cast<float>(static_cast<double>(m_stSettings.center[2]) - halfDepth + distance);
        pose.forward = {0.0F, 0.0F, 1.0F};
    }
    else if ((distance -= depth) < width)
    {
        // 奥側から -X 方向へ進む辺。
        pose.position[0]    =   static_cast<float>(static_cast<double>(m_stSettings.center[0]) + halfWidth - distance);
        pose.position[2]    =   static_cast<float>(static_cast<double>(m_stSettings.center[2]) + halfDepth);
        pose.forward = {-1.0F, 0.0F, 0.0F};
    }
    else
    {
        // 左端から -Z 方向へ進む辺。残った距離はこの辺上の移動量。
        distance -= width;
        pose.position[0]    =   static_cast<float>(static_cast<double>(m_stSettings.center[0]) - halfWidth);
        pose.position[2]    =   static_cast<float>(static_cast<double>(m_stSettings.center[2]) + halfDepth - distance);
        pose.forward = {0.0F, 0.0F, -1.0F};
    }

    // double から float への変換後も、返却する座標が有限値であることを保証する。
    if (!std::all_of(pose.position.begin(),
                     pose.position.end(),
                     [](const float i_value) {
                        return std::isfinite(i_value); 
                    }))
    {
        throw std::overflow_error("周回位置を表現できません。");
    }
    return pose;
}

/**
 * @brief 現在の周回寸法を返す。
 * @return X 方向の幅、Z 方向の奥行きの順の寸法（m）。
 */
std::array<float, 2> RectanglePatrol::Dimensions() const noexcept
{
    return {m_stSettings.width, m_stSettings.depth};
}

/**
 * @brief 現在の周回速度を返す。
 * @return 周回速度（m/s）。
 */
float RectanglePatrol::Speed() const noexcept
{
    return m_stSettings.speed;
}

/**
 * @brief 周回速度を設定する。
 * @param i_speed 0 以上の有限な移動速度（m/s）。
 * @throws std::invalid_argument 速度が負または有限でない場合。
 */
void RectanglePatrol::SetSpeed(const float i_speed)
{
    if (!std::isfinite(i_speed) || i_speed < 0.0F)
    {
        throw std::invalid_argument("周回速度は 0 以上の有限値で指定してください。");
    }
    m_stSettings.speed = i_speed;
}

}   // namespace demo::actor
