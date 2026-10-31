/**
 * @file wind_simulation.h
 * @brief 固定周期の風更新と CPU 側の風取得を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_SIMULATION_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_SIMULATION_H

#include "occupancy.h"
#include "wind_types.h"
#include "../wind_region.h"
#include <array>
#include <cstdint>
#include <memory>

namespace demo::wind::simulation
{
namespace windDefs = demo::wind::defs;

/**
 * @brief CPU で取得した風の種類。
 */
enum class WindSampleStatus
{
    Ready,      // GPU から取得済みのセルの風
    Preparing,  // 初回の読み戻しを待っている
    Fallback    // 現在または取得済みの管理空間の外
};

/**
 * @brief 管理空間の外における代替風の選択。
 */
enum class WindFallback
{
    BaseWind, // 最新の基本風
    Calm      // 無風
};

/**
 * @brief CPU が取得した風と、その由来。
 */
struct WindSample
{
    std::array<float, windDefs::kAxisCount> wind{};             // 風速ベクトル（m/s）
    WindSampleStatus     status = WindSampleStatus::Preparing;  // 取得状態
    std::uint64_t        frame = 0;                             // 取得元フレーム番号。代替風は 0
};

/**
 * @brief   管理空間、固定周期の風更新と非同期読み戻しをまとめる。
 * @details 全操作と破棄は同じ有効な OpenGL 4.3 コンテキストを持つスレッドで行う。
 */
class WindSimulation final
{
public:
    /**
     * @brief 初期位置、視線と基本風から管理空間と GPU 資源を作る。
     * @param i_settings 管理空間の寸法と追従条件。
     * @param i_position 基準点の初期位置。
     * @param i_forward 初期視線。
     * @param i_baseWind 初期基本風。
     * @throws std::invalid_argument 位置、視線、基本風または管理空間設定が不正な場合。
     * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
     */
    WindSimulation(const WindRegionSettings& i_settings,
                   const std::array<float, windDefs::kAxisCount>& i_position,
                   const std::array<float, windDefs::kAxisCount>& i_forward,
                   const std::array<float, windDefs::kAxisCount>& i_baseWind);
    ~WindSimulation();
    WindSimulation(const WindSimulation&) = delete;
    WindSimulation& operator=(const WindSimulation&) = delete;

    /**
     * @brief 1 秒あたりの更新回数を変更する。未処理の経過時間は保持する。
     * @param i_hertz 更新回数。正の有限値。
     * @throws std::invalid_argument 更新周期を表現できない場合。
     */
    void SetUpdateRate(double i_hertz);

    /**
     * @brief 60 Hz で 1 回更新する場合の分配割合を設定する。
     * @param i_fraction 0 以上 1 以下の割合。
     * @throws std::invalid_argument 割合が不正な場合。
     * @details 風速比例方式では 64 m/s のときの割合を指定する。
     */
    void SetDistribution(float i_fraction);

    /**
     * @brief 風速によらない方式と風速に比例する方式を切り替える。
     * @param i_method 分配方式。
     * @throws std::invalid_argument 方式が不正な場合。
     * @details 初期値は風速比例方式。
     */
    void SetDistributionMethod(WindDistribution i_method);

    /**
     * @brief 目標風との差が半分になる時間を設定する。
     * @param i_seconds 正の有限秒数。
     * @throws std::invalid_argument 時間が不正な場合。
     */
    void SetHalfLife(float i_seconds);

    /**
     * @brief 管理空間の追従、必要回数の風更新と非同期転送を行う。
     * @param i_elapsed 前回からの経過秒数。0 以上の有限値。
     * @param i_frame 1 以上の描画フレーム番号。前回より大きい値。
     * @param i_position 基準点の位置。
     * @param i_forward 基準点の視線。
     * @param i_baseWind 現在の基本風。
     * @param io_motors 反映する風発生器。
     * @param i_occupancy 占有率の精度。
     * @return 実行した風の更新回数。
     * @throws std::invalid_argument 経過時間、フレーム番号、位置、視線、基本風または精度が不正な場合。
     * @throws std::overflow_error 未処理時間または管理空間が表現範囲を超えた場合。
     * @throws std::runtime_error GPU 処理または読み戻しに失敗した場合。
     * @details 転送枠が全て使用中の場合は新しい転送を見送り、前回の取得済みデータを保持する。
     * 入力と追従先の検証後に失敗した場合は成功済みの更新と未処理時間を保持する。
     * 同じ経過時間を二重計上しないよう、再試行では 0 秒を渡す。
     * 描画プログラム、画像ユニット 0～5、シェーダーストレージの結合点 0～1 を変更する。
     */
    std::uint64_t Advance(double i_elapsed, std::uint64_t i_frame,
                          const std::array<float, windDefs::kAxisCount>& i_position,
                          const std::array<float, windDefs::kAxisCount>& i_forward,
                          const std::array<float, windDefs::kAxisCount>& i_baseWind,
                          motor::WindMotors& io_motors,
                          const OccupancySettings& i_occupancy = {});

    /**
     * @brief CPU の更新時間と非同期 GPU 時間を取得する。
     * @return 最後に取得した処理時間。GPU 時間は計測完了まで前回値を返す。
     */
    WindPerformanceStats Performance() const;

    /**
     * @brief 最後に完了した読み戻しからワールド座標の風を得る。
     * @param i_position 検索するワールド座標。
     * @param i_fallback 空間外または読み戻し前の範囲外に返す風。
     * @return 風速、状態、読み戻し元のフレーム番号。
     * @throws std::invalid_argument 座標または代替風の指定が不正な場合。
     */
    WindSample Sample(const std::array<double, windDefs::kAxisCount>& i_position,
                      WindFallback i_fallback = WindFallback::BaseWind) const;

    /**
     * @brief 現在の管理空間の原点を取得する。
     * @return XYZ 各軸の整数セル座標。
     */
    std::array<int, windDefs::kAxisCount> Origin() const;

    /**
     * @brief 現在の GPU 風速テクスチャを取得する。
     * @return XYZ 成分順の R32I テクスチャ名。
     */
    std::array<unsigned int, windDefs::kAxisCount> Textures() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_instImpl;
};
}
#endif
