/**
 * @file system_schedule.h
 * @brief 更新段階と System の実行順を管理する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_SYSTEM_SCHEDULE_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_SYSTEM_SCHEDULE_H

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

namespace demo::ecs
{
/** @brief 更新順に実行する段階。 */
enum class UpdatePhase
{
    PreUpdate,  // 更新前の段階
    Update,     // 更新中の段階
    PostUpdate, // 更新後の段階
    MAX         // 更新段階の数。実際の更新段階としては使用しない
};

/** @brief 段階ごとに優先度の高い System から逐次実行する。 */
class SystemSchedule final
{
public:
    /** @brief 空のスケジュールを作る。 */
    SystemSchedule() = default;
    SystemSchedule(const SystemSchedule&) = delete;
    SystemSchedule& operator=(const SystemSchedule&) = delete;
    SystemSchedule(SystemSchedule&&) = delete;
    SystemSchedule& operator=(SystemSchedule&&) = delete;

    /**
     * @brief System を登録する。同じ優先度の場合は登録順に実行する。
     * @param i_phase 実行する段階。
     * @param i_priority 大きいほど先に実行する優先度。
     * @param i_system 経過秒数を受け取る処理。
     * @throws std::invalid_argument 段階または処理が無効な場合。
     * @throws std::logic_error 実行中に登録した場合。
     */
    void Add(UpdatePhase i_phase, int i_priority, std::function<void(float)> i_system);

    /**
     * @brief 各段階の System を順番に実行する。
     * @param i_deltaSeconds 有限かつ 0 以上の経過秒数。
     * @throws std::invalid_argument 経過秒数が不正な場合。
     * @throws std::logic_error 再入した場合。
     */
    void Run(float i_deltaSeconds);

private:

    /**
     * @brief   System のエントリ。
     */
    struct Entry
    {
        int priority;                       // 優先度。大きいほど先に実行される。
        std::function<void(float)> system;  // 実行する System の処理。
    };
    std::array<std::vector<Entry>, static_cast<std::size_t>(UpdatePhase::MAX)> m_instSystems; // 段階ごとの System のリスト
    bool    m_bRunning = false;     // 実行中（更新中かどうか）
};
}
#endif
