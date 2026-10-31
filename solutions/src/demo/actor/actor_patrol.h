/**
 * @file actor_patrol.h
 * @brief XZ 平面上の四角形経路を周回する物体の設定と姿勢を定義する。
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ACTOR_ACTOR_PATROL_H
#define DYNAMIC_WIND_DEMO_DEMO_ACTOR_ACTOR_PATROL_H

#include <array>

namespace demo::actor
{

/**
 * @brief 四角形周回の中心、寸法、移動速度を指定する。
 * @details 配列の座標順は X、Y、Z。幅と奥行きはそれぞれ X 軸と Z 軸に沿う。
 */
struct PatrolSettings
{
    std::array<float, 3> center{}; // 周回長方形の中心座標（X、Y、Z）。
    float width = 10.0F;           // X 方向の幅（m）。
    float depth = 10.0F;           // Z 方向の奥行き（m）。
    float speed = 2.0F;            // 周回速度（m/s）。0 の場合は停止する。
};

/**
 * @brief 周回物体の位置と進行方向を表す。
 * @details 配列の座標順は X、Y、Z。進行方向は経路辺に沿った単位ベクトル。
 */
struct PatrolPose
{
    std::array<float, 3> position{};                // 物体原点のワールド座標（X、Y、Z）。
    std::array<float, 3> forward{1.0F, 0.0F, 0.0F}; // 現在の経路辺に沿う進行方向。
};

/**
 * @brief XZ 平面上の四角形経路に沿って物体を周回させる。
 * @details
 * 始点は中心から X、Z ともに負方向へ半寸法だけ離れた位置。
 * そこから +X、+Z、-X、-Z の順に進み、始点へ戻る。
 */
class RectanglePatrol final
{
public:
    /**
     * @brief 周回条件を検証して周回状態を初期化する。
     * @param i_settings 経路中心、寸法、速度。
     * @throws std::invalid_argument 中心、寸法、速度が不正な場合。
     */
    explicit RectanglePatrol(const PatrolSettings& i_settings);

    /**
     * @brief 経過時間に応じて周回位置を進める。
     * @param i_elapsed 経過秒数。
     * @return 更新後の位置と接線方向。
     * @throws std::invalid_argument 経過時間が負または有限でない場合。
     * @throws std::overflow_error 位置を表現できない場合。
     * @details 1 周を超えて進む場合は、余った距離を次の周回位置に反映する。
     */
    PatrolPose Advance(double i_elapsed);

    /**
     * @brief 周回中心を変更し、周回位置を新しい始点へ戻す。
     * @param i_center 物体原点が周回する高さと中心。
     * @throws std::invalid_argument 中心に有限でない値が含まれる場合。
     */
    void SetCenter(const std::array<float, 3>& i_center);

    /**
     * @brief 周回寸法を変更し、経路上の周回割合を保つ。
     * @param i_width X 方向の幅（m）。
     * @param i_depth Z 方向の奥行き（m）。
     * @throws std::invalid_argument 寸法が正の有限値でない場合。
     */
    void SetDimensions(float i_width, float i_depth);

    /**
     * @brief 周回状態を進めずに現在位置と進行方向を返す。
     * @return 現在の物体原点の座標と経路の進行方向。
     * @throws std::overflow_error 位置を表現できない場合。
     */
    PatrolPose Current() const;

    /**
     * @brief 現在の周回寸法を返す。
     * @return X 方向の幅、Z 方向の奥行きの順の寸法（m）。
     */
    std::array<float, 2> Dimensions() const noexcept;

    /**
     * @brief 現在の周回速度を返す。
     * @return 周回速度（m/s）。
     */
    float Speed() const noexcept;

    /**
     * @brief 移動速度を設定する。
     * @param i_speed 0 以上の有限な移動速度（m/s）。
     * @throws std::invalid_argument 速度が負または有限でない場合。
     */
    void SetSpeed(float i_speed);

private:
    PatrolSettings m_stSettings; // 現在の周回中心、寸法、速度。
    double m_fDistance = 0.0;    // 始点から経路に沿って進んだ距離（m）。
};

}
#endif
