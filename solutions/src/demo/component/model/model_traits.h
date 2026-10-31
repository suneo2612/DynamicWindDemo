/**
 * @file model_traits.h
 * @brief Model Component の識別子と依存関係を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_COMPONENT_MODEL_MODEL_TRAITS_H
#define DYNAMIC_WIND_DEMO_DEMO_COMPONENT_MODEL_MODEL_TRAITS_H

#include "../../ecs/component_catalog.h"
#include "../../ecs/component_lifecycle.h"
#include "model_component.h"
#include <array>

namespace demo::ecs
{
/**
 * @brief Model Component の静的な登録情報。
 */
template<>
struct ComponentTraits<component::model::ModelComponent>
{
    static constexpr auto kind          =   ComponentKind::Model;                       // コンポーネントの種類
    static constexpr auto dependencies  =   std::array{ ComponentKind::Transform };     // 依存するコンポーネントの種類
    static constexpr auto priority      =   0;                                          // 優先度

    // 明示的に初期化不要であることを示す
    using Initializer = NoOpComponentInitializer<component::model::ModelComponent>;
};
}
#endif
