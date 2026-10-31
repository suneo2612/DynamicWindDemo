/**
 * @file component_world_types.h
 * @brief ComponentWorld 内部の格納情報を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_DEFS_COMPONENT_WORLD_TYPES_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_DEFS_COMPONENT_WORLD_TYPES_H

#include "../component_catalog.h"
#include "../entity_registry.h"
#include <vector>

namespace demo::ecs::detail
{

/**
 * @brief Entity に対応する Component の値。
 * @tparam T 格納する Component 型。
 */
template<typename T>
struct WorldStored
{
    Entity entity; // 対応する Entity
    T value;       // Component の値
};

/**
 * @brief Entity に登録された Component の初期化順。
 */
struct WorldRecord
{
    Entity entity;                    // 対応する Entity
    std::vector<ComponentKind> order; // 初期化の順序
};

}
#endif
