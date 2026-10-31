/**
 * @file transform_traits.h
 * @brief Transform Component の識別子と依存関係を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_COMPONENT_TRANSFORM_TRANSFORM_TRAITS_H
#define DYNAMIC_WIND_DEMO_DEMO_COMPONENT_TRANSFORM_TRANSFORM_TRAITS_H

#include "../../ecs/component_catalog.h"
#include "../../ecs/component_lifecycle.h"
#include "transform_system.h"
#include <array>

namespace demo::ecs
{
/**
 *  @brief Transform Component の静的な登録情報。
 */
template<>
struct ComponentTraits<component::transform::LocalTransform>
{
    static constexpr auto kind          =   ComponentKind::Transform;                   // コンポーネントの種類
    static constexpr auto dependencies  =   std::array<ComponentKind, 0>{};             // 依存するコンポーネントの種類（なし）
    static constexpr auto priority      =   0;                                          // 更新の優先度
    using Initializer = NoOpComponentInitializer<component::transform::LocalTransform>; // 初期化子は不要物であることを明示
};
}   // namespace demo::ecs
#endif
