/**
 * @file entity_registry.cpp
 * @brief Entity 識別子の再利用と有効性検査を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "entity_registry.h"
#include <stdexcept>

namespace demo::ecs
{

/**
 * @brief 識別子同士を比較する。
 * @param i_other 比較対象。
 * @return 生成元、番号および世代が同じ場合に true。
 */
bool Entity::operator==(const Entity& i_other) const noexcept
{
    return  m_uIndex == i_other.m_uIndex &&
            m_uGeneration == i_other.m_uGeneration &&
            !m_instOwner.owner_before(i_other.m_instOwner) &&
            !i_other.m_instOwner.owner_before(m_instOwner);
}

/**
 * @brief 独立した識別子の管理元を作る。
 */
EntityRegistry::EntityRegistry()
    : m_instIdentity(std::make_shared<const unsigned char>(0))
{
}

/**
 * @brief 新しい Entity を発行する。
 * @return 管理元内で有効な識別子。
 * @throws std::overflow_error 識別子の番号を使い切った場合。
 */
Entity EntityRegistry::Create()
{
    // 新しい Entity を作成してEntityRegistryに登録する。
    auto entity = Entity();
    entity.m_instOwner = m_instIdentity;    // ハンドルの管理元を弱参照として設定する
    if (m_uFreeHead != std::numeric_limits<std::uint32_t>::max())
    {
        // 空きスロットが存在する場合はそこを利用する。
        entity.m_uIndex = m_uFreeHead;
        m_uFreeHead = m_instSlots[entity.m_uIndex].next;
    }
    else
    {
        // 新規スロットを割り当てる
        if (m_instSlots.size() >= std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error("Entity の識別子を使い切りました。");
        }
        entity.m_uIndex = static_cast<std::uint32_t>(m_instSlots.size());
        m_instSlots.emplace_back();
    }

    // スロットを取得して起動状態を更新する
    auto& slot = m_instSlots[entity.m_uIndex];
    slot.alive = true;
    entity.m_uGeneration = slot.generation;
    return entity;
}

/**
 * @brief 有効な Entity の識別子を無効化する。
 * @param i_entity 無効化する識別子。
 * @throws std::invalid_argument 無効または別の管理元の識別子の場合。
 */
void EntityRegistry::Destroy(const Entity& i_entity)
{
    if (!IsAlive(i_entity))
    {
        throw std::invalid_argument("無効な Entity は削除できません。");
    }

    // 指定の Entity に対応するスロットを取得する
    auto& slot = m_instSlots[i_entity.m_uIndex];
    if (slot.generation != std::numeric_limits<std::uint32_t>::max())
    {
        ++slot.generation;                  // 生まれ変わりの世代番号を更新する
        slot.next = m_uFreeHead;            // 空きスロット連結リストに追加する
        m_uFreeHead = i_entity.m_uIndex;    // 次の空きスロットとして登録する    
    }
    slot.alive = false;
}

/**
 * @brief 生成元と世代を含めて有効性を検査する。
 * @param i_entity 検査する Entity。
 * @return 現在有効な場合に true。
 */
bool EntityRegistry::IsAlive(const Entity& i_entity) const noexcept
{
    const auto owner = i_entity.m_instOwner.lock();
    return  owner && 
            owner == m_instIdentity &&
            i_entity.m_uIndex < m_instSlots.size() &&
            m_instSlots[i_entity.m_uIndex].alive &&
            m_instSlots[i_entity.m_uIndex].generation == i_entity.m_uGeneration;
}
}
