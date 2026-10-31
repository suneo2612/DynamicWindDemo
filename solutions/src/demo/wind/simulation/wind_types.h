/**
 * @file wind_types.h
 * @brief 風格子とシミュレーションに共通する設定と統計。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_TYPES_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_TYPES_H

namespace demo::wind::simulation
{
/**
 * @brief 隣接セルへ風を送る割合の決め方。
 */
enum class WindDistribution
{
    Constant,       // 風速によらず一定の割合
    SpeedDependent  // 風速 64 m/s のときに設定した割合
};

/**
 * @brief 風の更新に関する CPU 時間と、非同期取得した GPU 時間。
 * @details
 * GPU 時間は最後に取得した完了済みの計測値であり、現在のフレームや最新の実行に対応するとは限らない。
 * motorCacheRebuilt は WindGrid では最後の発生器付き更新で再計算が必要と判定されたかを示す。
 * この判定は GPU 更新前に記録されるため、更新失敗時も true になり得る。
 * WindSimulation では最後に正常終了した Advance() 内のいずれかの更新で再計算したかを示し、
 * 更新がなかった場合は false になる。
 */
struct WindPerformanceStats
{
    double  cpuUpdateMilliseconds   =   0.0;    // 最後に正常終了した Advance() の経過時間（ms、待機時間を含む）
    double  gpuMotorMilliseconds    =   0.0;    // 最後に取得した発生器寄与計算の GPU 時間（ms）
    double  gpuStepMilliseconds     =   0.0;    // 最後に取得した風更新の GPU 時間（ms）
    bool    hasCpuUpdate            =   false;  // Advance() の経過時間を取得済みか
    bool    hasGpuMotor             =   false;  // 発生器寄与計算の GPU 時間を取得済みか
    bool    hasGpuStep              =   false;  // 風更新の GPU 時間を取得済みか
    bool    motorCacheRebuilt       =   false;  // 発生器の寄与を再計算したか、または再計算が必要と判定されたか
};
}
#endif
