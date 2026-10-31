/**
 * @file wind_grid.h
 * @brief GPU 上の風速格子と隣接セルへの伝播を定義する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_GRID_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_GRID_H

#include <array>
#include <cstdint>
#include <memory>
#include "occupancy.h"
#include "wind_types.h"
#include "../defs/wind_region_constants.h"

namespace demo::wind::simulation
{
namespace windDefs = demo::wind::defs;

/**
 * @brief   GPU 上の 3D テクスチャに風速を保持し、隣接セルへ伝える。
 * @details
 *  作成、更新、破棄は同じ有効な OpenGL 4.3 コンテキスト上で行う。
 *  管理空間の移動と CPU 側の取得は WindSimulation を通して利用できる。
 */
class WindGrid final
{
public:
    /**
     * @brief 指定したセル数で風速を初期化する。
     * @param i_size X、Y、Z のセル数。
     * @param i_initial 初期風速（m/s）。
     * @throws std::invalid_argument セル数または風速が不正な場合。
     * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
     */
    WindGrid(const std::array<int, windDefs::kAxisCount>& i_size,
             const std::array<float, windDefs::kAxisCount>& i_initial);
    ~WindGrid();
    WindGrid(const WindGrid&) = delete;
    WindGrid& operator=(const WindGrid&) = delete;

    /**
     * @brief 風を一度隣接セルへ伝播し、基本風へ近づける。
     * @param i_seconds 風の更新時間（秒）。
     * @param i_baseWind 空間外から流入する風と減衰先（m/s）。
     * @throws std::invalid_argument 時間または風速が不正な場合。
     * @throws std::runtime_error GPU への処理要求が失敗した場合。
     * @details 初期方式は風速比例で、64 m/s のとき 60 Hz で 25% を分配する。
     * 目標風との差の半減時間の初期値は 2 秒。
     * この呼び出しはプログラム、画像ユニット 0～5 の設定を変更する。
     */
    void Step(float i_seconds, const std::array<float, windDefs::kAxisCount>& i_baseWind);

    /**
     * @brief 発生器の占有率を反映して風を一度更新する。
     * @param i_seconds 風の更新時間（秒）。
     * @param i_baseWind 基本風（m/s）。
     * @param i_origin 管理空間の最小ワールド座標（整数メートル）。
     * @param io_motors 風を反映する発生器。成功後に有効な瞬間発生器を解放する。
     * @param i_settings 占有率の近似精度。
     * @throws std::invalid_argument 時間、風速、座標または占有率設定が不正な場合。
     * @throws std::runtime_error GPU 処理または完了待ちに失敗した場合。
     * @details 呼び出し中に同じ発生器管理器を他のスレッドから変更してはならない。
     * セルを各辺 1～4 分割し、小立方体の中心の内外から占有率を近似する。
     * 薄い重なりは標本点に当たらず見逃す場合がある。
     * 瞬間発生器がある場合は GPU 完了を待つ。シェーダーストレージの結合点 0 と 1 を変更する。
     */
    void Step(float i_seconds, const std::array<float, windDefs::kAxisCount>& i_baseWind,
              const std::array<int, windDefs::kAxisCount>& i_origin, motor::WindMotors& io_motors,
              const OccupancySettings& i_settings = {});

    /**
     * @brief 最後に完了した GPU タイマー計測値を取得する。
     * @return 発生器寄与計算と風更新の GPU 時間。
     */
    WindPerformanceStats Performance() const;

    /**
     * @brief 現在の風速テクスチャを X、Y、Z 成分順に取得する。
     * @return R32I 形式の OpenGL テクスチャ名。値は符号付き 16.16 固定小数点。
     * @details 更新後は再取得する。所有権は移らず、破棄や変更をしてはならない。
     */
    std::array<unsigned int, windDefs::kAxisCount> Textures() const;

    /**
     * @brief 管理空間を移動し、重複領域の風をワールド座標に対応させて保持する。
     * @param i_offset XYZ 各軸の移動セル数。
     * @param i_baseWind 新たに管理範囲へ入るセルの風（m/s）。
     * @throws std::invalid_argument 基本風が不正な場合。
     * @throws std::runtime_error GPU 処理に失敗した場合。
     */
    void Move(const std::array<int, windDefs::kAxisCount>& i_offset,
              const std::array<float, windDefs::kAxisCount>& i_baseWind);

    /**
     * @brief 60 Hz で 1 回更新する場合の分配割合を設定する。
     * @param i_fraction 0 以上 1 以下の割合。
     * @throws std::invalid_argument 割合が範囲外または非有限の場合。
     * @details 風速比例方式では 64 m/s のときの割合を指定する。
     */
    void SetDistribution(float i_fraction);

    /**
     * @brief 風速によらない方式と風速に比例する方式を切り替える。
     * @param i_method 分配方式。
     * @throws std::invalid_argument 方式が不正な場合。
     * @details 初期値は風速比例方式。境界の基本風にも同じ方式を適用する。
     */
    void SetDistributionMethod(WindDistribution i_method);

    /**
     * @brief 目標風との差が半分になる時間を設定する。
     * @param i_seconds 正の有限秒数。
     * @throws std::invalid_argument 時間が不正な場合。
     */
    void SetHalfLife(float i_seconds);

private:
    struct MotorInput;
    void StepWithMotors(float i_seconds, const std::array<float, windDefs::kAxisCount>& i_baseWind,
                        const MotorInput* i_input);
    struct Impl;
    std::unique_ptr<Impl> m_instImpl;
};


}

// 既存の利用側がこのヘッダーだけで WindSimulation を参照できるようにする。
#include "wind_simulation.h"
#endif
