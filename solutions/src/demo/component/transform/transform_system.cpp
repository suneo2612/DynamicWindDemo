/**
 * @file transform_system.cpp
 * @brief Transform の階層とワールド変換の同期を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */

#include "transform_system.h"
#include <algorithm>
#include <stdexcept>

namespace demo::component::transform
{
namespace
{
using general::math::Matrix4;   // 4x4 行列
using general::vec::Vector3;    // 3次元ベクトル
}

/**
 * @brief Entity の管理元を指定する。
 * @param i_registry    この System より長く生存する管理元。
 */
TransformSystem::TransformSystem(const ecs::EntityRegistry& i_registry) noexcept
    : m_instRegistry(i_registry)
{
}

/**
 * @brief エントリを検索する。
 * @param i_entity 対象。
 * @return エントリのインデックス。
 */
std::size_t TransformSystem::Find(const ecs::Entity& i_entity) const
{
    if (!m_instRegistry.IsAlive(i_entity)) { throw std::invalid_argument("無効な Entity は使用できません。"); }

    // 処理するEntityを取得
    const auto found = std::find_if(
        m_instEntries.begin(),
        m_instEntries.end(),
        [&i_entity](const Entry& i_entry) {
            return i_entry.entity == i_entity;
        }
    );

    // Transform 無いに登録がない場合は例外を投げる
    if (found == m_instEntries.end()) { throw std::invalid_argument("Entity に Transform がありません。");}
    return static_cast<std::size_t>(found - m_instEntries.begin());
}

/**
 * @brief 有効な Entity に Transform を追加する。
 * @param i_entity 対象。
 * @param i_transform 初期ローカル姿勢。
 * @throws std::invalid_argument Entity または姿勢が不正な場合。
 * @throws std::logic_error 既に Transform を保持している場合。
 */
void TransformSystem::Add(const ecs::Entity& i_entity, const LocalTransform& i_transform)
{
    if (!m_instRegistry.IsAlive(i_entity)) { throw std::invalid_argument("無効な Entity に Transform を追加できません。"); }

    // 指定した Entity が既に Transform を持っていないかm_instEntriesの登録有無で確認する
    if (std::any_of(
        m_instEntries.begin(),
        m_instEntries.end(),
        [&i_entity](const Entry& i_entry) {
            return i_entry.entity == i_entity;
        }))
    {
        throw std::logic_error("Entity には既に Transform があります。");
    }

    // ワールド行列を計算してエラーが出ないかチェックしてからエントリに追加する
    static_cast<void>(Matrix4::Transform(i_transform.position, i_transform.rotation, i_transform.scale));
    m_instEntries.push_back(Entry{ i_entity, std::nullopt, i_transform, {}, {}, true });
}

/**
 * @brief 親子関係を持たない Entity の Transform を削除する。
 * @param i_entity 対象。
 * @throws std::logic_error 親子関係が残っている場合。
 * @throws std::invalid_argument Entity または Transform が無効な場合。
 */
void TransformSystem::Remove(const ecs::Entity& i_entity)
{
    // Entity が子を持っている場合は削除できないのでチェックする
    const auto index = Find(i_entity);
    if (m_instEntries[index].parent ||
        std::any_of(
            m_instEntries.begin(),
            m_instEntries.end(),
            [&i_entity](const Entry& i_entry)
            {
                return i_entry.parent && *i_entry.parent == i_entity;
            }
        ))
    {
        throw std::logic_error("親子関係のある Transform は単独で削除できません。");
    }
    m_instEntries.erase(m_instEntries.begin() + static_cast<std::ptrdiff_t>(index));
}


// ============================================================================
// 姿勢と親子関係のセット
// ============================================================================
/**
 * @brief ローカル姿勢を設定し、自身と子孫を未同期にする。
 * @param i_entity 対象。
 * @param i_transform 新しい姿勢。
 * @throws std::invalid_argument Entity または姿勢が不正な場合。
 */
void TransformSystem::SetLocal(const ecs::Entity& i_entity, const LocalTransform& i_transform)
{
    // 指定した Entity を取得し、セットする新しい姿勢にも問題がないか確認する
    const auto index = Find(i_entity);
    static_cast<void>(Matrix4::Transform(i_transform.position, i_transform.rotation, i_transform.scale));
    m_instEntries[index].local = i_transform;

    // 姿勢更新につき未同期扱いにする
    MarkDescendants(i_entity);
}

/**
 * @brief ローカル姿勢を維持したまま親を変更する。
 * @param i_entity 子 Entity。
 * @param i_parent 新しい親。親を解除する場合は std::nullopt。
 * @throws std::invalid_argument Entity が無効、または循環が生じる場合。
 */
void TransformSystem::SetParent(const ecs::Entity& i_entity, const std::optional<ecs::Entity>& i_parent)
{
    const auto index = Find(i_entity);
    
    // 末端の親まで辿って循環がないか確認する
    auto ancestor = i_parent;
    while (ancestor)
    {
        if (*ancestor == i_entity)
        {
            throw std::invalid_argument("Transform の親子関係が循環しています。");
        }
        ancestor = m_instEntries[Find(*ancestor)].parent;
    }
    m_instEntries[index].parent = i_parent;

    // 親を変更したので自身のワールド行列も未同期扱いにする
    MarkDescendants(i_entity);
}

/**
 * @brief 現在の Transform 親を取得する。
 * @param i_entity 対象。
 * @return 親 Entity。親がなければ std::nullopt。
 * @throws std::invalid_argument Entity または Transform が無効な場合。
 */
std::optional<ecs::Entity> TransformSystem::GetParent(const ecs::Entity& i_entity) const
{
    return m_instEntries[Find(i_entity)].parent;
}


// ============================================================================
// 同期対応
// ============================================================================
/**
 * @brief 子孫のエントリを更新が必要な状態にする。
 * @param i_entity 対象。
 */
void TransformSystem::MarkDescendants(const ecs::Entity& i_entity)
{
    // 自身を未同期扱いにする
    m_instEntries[Find(i_entity)].dirty = true;

    // 子孫も未同期扱いにする
    auto changed = true;
    while (changed)
    {
        changed = false;
        for (auto& entry : m_instEntries)
        {
            if (!entry.dirty && entry.parent && m_instEntries[Find(*entry.parent)].dirty)
            {
                entry.dirty = true;
                changed = true;
            }
        }
    }
}

/**
 * @brief 親から子の順に未同期のワールド変換をまとめて計算する。
 * @throws std::invalid_argument 管理中の Entity が既に無効な場合。
 * @throws std::overflow_error 行列の演算結果が表現可能な範囲外の場合。
 */
void TransformSystem::Synchronize()
{
    // 未同期のエントリを数える
    auto pending    =   m_instEntries;
    auto remaining  =   std::size_t{};
    for (const auto& entry : pending)
    {
        if (!m_instRegistry.IsAlive(entry.entity))
        {
            throw std::invalid_argument("Transform が保持する Entity は既に無効です。");
        }
        if (entry.dirty) { ++remaining; }
    }

    // 未同期が残っている限り処理を続ける
    while (remaining > 0)
    {
        auto progress = false;
        for (auto& entry : pending)
        {
            if (!entry.dirty) { continue; }

            // 親を先に更新しないと意味がないので一旦スキップ
            auto parentIndex = std::size_t{};
            if (entry.parent)
            {
                parentIndex = Find(*entry.parent);
                if (pending[parentIndex].dirty) { continue; }
            }

            // ローカル変換をワールド変換に反映する
            const auto local    =   Matrix4::Transform(entry.local.position, entry.local.rotation, entry.local.scale);
            entry.world         =   entry.parent ? pending[parentIndex].world * local : local;
            entry.worldRotation =   entry.parent ? pending[parentIndex].worldRotation * entry.local.rotation :
                                                   entry.local.rotation;
            entry.dirty = false;
            --remaining;
            progress = true;
        }
        if (!progress)
        {
            throw std::logic_error("Transform の親子関係を同期できません。");
        }
    }

    // 計算結果を元のエントリに差し替える
    m_instEntries.swap(pending);
}

/**
 * @brief 同期済みのエントリを取得する。
 * @param i_entity 対象。
 * @return 同期済みのエントリ。
 * @throws std::logic_error 対象が未同期の場合。
 */
const TransformSystem::Entry& TransformSystem::Synchronized(const ecs::Entity& i_entity) const
{
    const auto& entry = m_instEntries[Find(i_entity)];
    if (entry.dirty)
    {
        throw std::logic_error("未同期のワールド変換は取得できません。");
    }

    // 親が生きているか確認する
    auto ancestor = entry.parent;
    while (ancestor)
    {
        ancestor = m_instEntries[Find(*ancestor)].parent;
    }
    return entry;
}


// ============================================================================
//  各種パラメータ取得
// ============================================================================
/**
 * @brief ローカル姿勢を値として返す。
 * @param i_entity 対象。
 * @return ローカル姿勢。
 * @throws std::invalid_argument Entity または Transform が無効な場合。
 */
LocalTransform TransformSystem::GetLocal(const ecs::Entity& i_entity) const
{
    return m_instEntries[Find(i_entity)].local;
}

/**
 * @brief ローカル姿勢における前方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない前方向。
 */
Vector3 TransformSystem::GetLocalForward(const ecs::Entity& i_entity) const
{
    return GetLocal(i_entity).rotation.Rotate(Vector3(0, 0, -1));
}

/**
 * @brief ローカル姿勢における右方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない右方向。
 */
Vector3 TransformSystem::GetLocalRight(const ecs::Entity& i_entity) const
{
    return GetLocal(i_entity).rotation.Rotate(Vector3(1, 0, 0));
}

/**
 * @brief ローカル姿勢における上方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない上方向。
 */
Vector3 TransformSystem::GetLocalUp(const ecs::Entity& i_entity) const
{
    return GetLocal(i_entity).rotation.Rotate(Vector3(0, 1, 0));
}

/**
 * @brief 同期済みのワールド行列を返す。
 * @param i_entity 対象。
 * @return 列優先のワールド変換。
 * @throws std::logic_error 対象が未同期の場合。
 * @throws std::invalid_argument 対象または親 Entity が無効な場合。
 */
Matrix4 TransformSystem::GetWorldMatrix(const ecs::Entity& i_entity) const
{
    return Synchronized(i_entity).world;
}

/**
 * @brief 同期済みのワールド位置を返す。
 * @param i_entity 対象。
 * @return ワールド座標の位置。
 * @throws std::logic_error 対象が未同期の場合。
 */
Vector3 TransformSystem::GetWorldPosition(const ecs::Entity& i_entity) const
{
    const auto& data = Synchronized(i_entity).world.Data();
    return Vector3(data[12], data[13], data[14]);
}

/**
 * @brief 同期済みのワールド姿勢を返す。
 * @param i_entity 対象。
 * @return 親から子へ合成した、スケールを含まない四元数。
 * @throws std::logic_error 対象が未同期の場合。
 */
general::math::Quaternion TransformSystem::GetWorldRotation(const ecs::Entity& i_entity) const
{
    return Synchronized(i_entity).worldRotation;
}

/**
 * @brief 同期済みのワールド姿勢における前方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない前方向。
 * @throws std::logic_error 対象が未同期の場合。
 */
Vector3 TransformSystem::GetWorldForward(const ecs::Entity& i_entity) const
{
    return Synchronized(i_entity).worldRotation.Rotate(Vector3(0, 0, -1));
}

/**
 * @brief 同期済みのワールド姿勢における右方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない右方向。
 * @throws std::logic_error 対象が未同期の場合。
 */
Vector3 TransformSystem::GetWorldRight(const ecs::Entity& i_entity) const
{
    return Synchronized(i_entity).worldRotation.Rotate(Vector3(1, 0, 0));
}

/**
 * @brief 同期済みのワールド姿勢における上方向を返す。
 * @param i_entity 対象。
 * @return スケールを含まない上方向。
 * @throws std::logic_error 対象が未同期の場合。
 */
Vector3 TransformSystem::GetWorldUp(const ecs::Entity& i_entity) const
{
    return Synchronized(i_entity).worldRotation.Rotate(Vector3(0, 1, 0));
}
}
