/**
 * @file model_component.h
 * @brief Entity と共有描画モデルを関連付ける。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_COMPONENT_MODEL_MODEL_COMPONENT_H
#define DYNAMIC_WIND_DEMO_DEMO_COMPONENT_MODEL_MODEL_COMPONENT_H

#include <memory>

namespace render { class Model; }

namespace demo::component::model
{
/** @brief Entity が表示する共有モデル。空の場合は描画しない。 */
struct ModelComponent
{
    std::shared_ptr<const render::Model> model;
};
}
#endif
