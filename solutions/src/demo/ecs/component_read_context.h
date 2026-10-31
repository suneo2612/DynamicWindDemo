/**
 * @file component_read_context.h
 * @brief Component の初期化・終了時の読み取りコンテキストを定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_READ_CONTEXT_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_READ_CONTEXT_H

#include "component_catalog.h"
#include "entity_registry.h"
#include <concepts>
#include <optional>
#include <tuple>

namespace demo::ecs
{

template<Component... Components>
class ComponentWorld;

namespace detail
{

/**
 * @brief   初期化・終了処理が他の Component を読み取るためのコンテキスト。
 * @tparam  Components World に登録された Component 型。
 * @details
 * 同時追加中の Component も読めるが、同じ段階でまだ処理されていない
 * Component の処理結果には依存しないこと。返却された参照を保持してはならない。
 */
template<Component... Components>
class ComponentReadContext final
{
public:
    /**
     * @brief 対象 Entity の Component を参照する。
     * @tparam T 参照する Component 型。
     * @return 対象 Component の読み取り専用参照。
     * @throws std::invalid_argument 対象 Component が存在しない場合。
     */
    template<typename T> requires (std::same_as<T, Components> || ...)
    const T& Get() const
    {
        return Get<T>(m_instEntity);
    }

    /**
     * @brief 指定した Entity の Component を参照する。
     * @tparam T 参照する Component 型。
     * @param i_entity 参照する Entity。
     * @return Component の読み取り専用参照。
     * @throws std::invalid_argument Entity または Component が無効な場合。
     */
    template<typename T> requires (std::same_as<T, Components> || ...)
    const T& Get(const Entity& i_entity) const
    {
        if (i_entity == m_instEntity && m_pStaged)
        {
            const auto& staged = std::get<std::optional<T>>(*m_pStaged);
            if (staged) { return *staged; }
        }
        return m_instWorld.template Get<T>(i_entity);
    }

private:

    // 管理部分からのアクセス許可
    friend class ComponentWorld<Components...>;
    using Staged = std::tuple<std::optional<Components>...>;

    /**
     * @brief 指定した Entity と追加中の値を参照するコンテキストを作る。
     * @param i_world 登録済み Component の参照元。
     * @param i_entity 初期化または終了する Entity。
     * @param i_staged 追加中の Component。追加中でなければ nullptr。
     */
    ComponentReadContext(const ComponentWorld<Components...>& i_world,
                         const Entity& i_entity,
                         const Staged* i_staged)
        : m_instWorld(i_world), m_instEntity(i_entity), m_pStaged(i_staged) {}

    const ComponentWorld<Components...>& m_instWorld; // 登録済み Component の参照元
    Entity m_instEntity;                              // 初期化または終了の対象
    const Staged* m_pStaged;                          // 追加中の Component
};

}   // namespace detail
}   // namespace demo::ecs
#endif
