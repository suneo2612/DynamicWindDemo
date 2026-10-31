/**
 * @file system_schedule.cpp
 * @brief System の順序を制御する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "system_schedule.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace demo::ecs
{

/**
 * @brief System を登録する。同じ優先度の場合は登録順に実行する。
 * @param i_phase 実行する段階。
 * @param i_priority 大きいほど先に実行する優先度。
 * @param i_system 経過秒数を受け取る処理。
 * @throws std::invalid_argument 段階または処理が無効な場合。
 * @throws std::logic_error 実行中に登録した場合。
 */
void SystemSchedule::Add(const UpdatePhase          i_phase,
                         const int                  i_priority,
                         std::function<void(float)> i_system)
{
    // 更新中は登録できない
    if (m_bRunning)
    {
        throw std::logic_error("更新中は System を登録できません。");
    }

    // 段階と処理の妥当性をチェックする
    const auto index = static_cast<unsigned int>(i_phase);
    if (index >= m_instSystems.size() || !i_system)
    {
        throw std::invalid_argument("更新段階または System が不正です。");
    }


    auto& systems = m_instSystems[index];
    const auto position = std::find_if(systems.begin(), systems.end(),
        [i_priority](const Entry& i_entry) { return i_entry.priority < i_priority; });
    systems.insert(position, Entry{ i_priority, std::move(i_system) });
}

/**
 * @brief   各段階の System を順番に実行する。
 * @param   i_deltaSeconds 経過秒数。
 * @throws  std::invalid_argument 経過秒数が不正な場合。
 * @throws  std::logic_error 再入した場合。
 */
void SystemSchedule::Run(const float i_deltaSeconds)
{
    if (!std::isfinite(i_deltaSeconds) || i_deltaSeconds < 0.0F)
    {
        throw std::invalid_argument("経過時間は有限かつ 0 以上で指定してください。");
    }

    // 実行中フラグをチェックして再入を防ぐ
    if (m_bRunning)
    {
        throw std::logic_error("System の更新を再入できません。");
    }

    /**
     * @brief   実行中フラグを管理するガード。
     * @details 実行中フラグを true に設定し、スコープを抜けると自動的に false に戻す。
     */
    struct RunningGuard
    {
        bool& running;
        ~RunningGuard() { running = false; }
    };
    m_bRunning = true;
    const auto guard = RunningGuard{ m_bRunning };
    for (const auto& phase : m_instSystems)
    {
        for (const auto& entry : phase)
        {
            entry.system(i_deltaSeconds);
        }
    }
}
}
