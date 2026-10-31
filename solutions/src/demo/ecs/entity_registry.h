/**
 * @file entity_registry.h
 * @brief Entity の識別子と世代を管理する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_ENTITY_REGISTRY_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_ENTITY_REGISTRY_H

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace demo::ecs
{

class EntityRegistry;

/**
 * @brief 生成元と世代を含む Entity の識別子。
 */
class Entity final
{
public:
    /**
     * @brief 無効な識別子を作る。
     */
    Entity() noexcept = default;

    /**
     * @brief 識別子同士を比較する。
     * @param i_other 比較対象。
     * @return 生成元、番号および世代が同じ場合に true。
     */
    bool operator==(const Entity& i_other) const noexcept;

private:
    friend class EntityRegistry;
    std::weak_ptr<const unsigned char> m_instOwner;                           // 生成元の識別情報
    std::uint32_t m_uIndex = std::numeric_limits<std::uint32_t>::max();       // 管理元内のスロット番号
    std::uint32_t m_uGeneration = 0;                                          // スロットの世代
};


/**
 * @brief 古い識別子や別の管理元の識別子を検出する。
 */
class EntityRegistry final
{
public:
    /** 
     * @brief 独立した識別子の管理元を作る。（コピー不可, ムーブ不可）
     */
    EntityRegistry();
    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;
    EntityRegistry(EntityRegistry&&) = delete;
    EntityRegistry& operator=(EntityRegistry&&) = delete;

    /**
     * @brief 新しい Entity を発行する。
     * @return 管理元内で有効な識別子。
     * @throws std::overflow_error 識別子の番号を使い切った場合。
     */
    Entity Create();

    /**
     * @brief 有効な Entity の識別子を無効化する。
     * @param i_entity 無効化する識別子。
     * @throws std::invalid_argument 無効または別の管理元の識別子の場合。
     */
    void Destroy(const Entity& i_entity);

    /**
     * @brief 生成元と世代を含めて有効性を検査する。
     * @param i_entity 検査する識別子。
     * @return 現在有効な場合に true。
     */
    bool IsAlive(const Entity& i_entity) const noexcept;

private:

    /**
     * @brief スロットの構造体。
     */
    struct Slot
    {
        std::uint32_t generation = 1;                                         // スロットの現在の世代
        std::uint32_t next = std::numeric_limits<std::uint32_t>::max();       // 空きスロット連結先（初期値は無効値として数値表現の最大値）
        bool alive = false;                                                   // Entity が有効かどうか
    };

    std::shared_ptr<const unsigned char> m_instIdentity;                       // この管理元を識別する共有トークン
    std::vector<Slot> m_instSlots;                                             // Entity のスロット一覧
    std::uint32_t m_uFreeHead = std::numeric_limits<std::uint32_t>::max();     // 再利用する空きスロットの先頭
};

}
#endif
