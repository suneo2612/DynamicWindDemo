/**
 * @file wind_readback.h
 * @brief GPU の風格子を非同期に読み戻し、取得済みの値を参照する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_READBACK_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_READBACK_H

#include "wind_simulation.h"
#include <array>
#include <cstdint>
#include <memory>

namespace demo::wind::simulation::detail
{
/**
 * @brief 転送枠、取得済みの風とその原点・フレームを所有する。
 * @details 作成、使用、破棄は同じ有効な OpenGL コンテキスト上で行う。
 */
class WindReadback final
{
public:
    /**
     * @brief 読み戻しサイズと初期原点を検証する。
     * @param i_size 格子の寸法。
     * @param i_origin 初期の管理空間の原点。
     */
    WindReadback(const std::array<int, windDefs::kAxisCount>& i_size,
                 const std::array<int, windDefs::kAxisCount>& i_origin);
    ~WindReadback();
    WindReadback(const WindReadback&) = delete;
    WindReadback& operator=(const WindReadback&) = delete;

    /**
     * @brief 完了した転送だけを回収する。
     */
    void Poll();

    /**
     * @brief 空き枠がある場合に風格子の非同期読み戻しを開始する。
     * @param i_textures 格子の寸法に一致する XYZ 成分順の R32I 形式の 3D テクスチャ名。
     * @param i_origin 転送元の管理空間の原点。
     * @param i_frame 転送元の描画フレーム番号。1 以上で、前回の呼び出しより大きい値。
     * @throws std::runtime_error バッファーやフェンスの作成、転送の開始に失敗した場合。
     * @details 空き枠がない場合は転送を見送り、取得済みデータを保持する。
     * 完了したデータは後の Poll() で回収する。呼び出し元のピクセル転送設定と
     * 3D テクスチャの結合は復元する。
     * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    void Submit(const std::array<unsigned int, windDefs::kAxisCount>& i_textures,
                const std::array<int, windDefs::kAxisCount>& i_origin, std::uint64_t i_frame);

    /**
     * @brief 現在の空間と取得済みデータに基づいてワールド座標の風を返す。
     * @param i_position 取得する座標。
     * @param i_origin 現在の管理空間の原点。
     * @param i_baseWind 現在の基本風。
     * @param i_fallback 範囲外の代替風。
     * @return 風速と取得状態。
     */
    WindSample Sample(const std::array<double, windDefs::kAxisCount>& i_position,
                      const std::array<int, windDefs::kAxisCount>& i_origin,
                      const std::array<float, windDefs::kAxisCount>& i_baseWind,
                      WindFallback i_fallback) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_instImpl; // 転送資源と最後に取得したデータ。
};
}
#endif
