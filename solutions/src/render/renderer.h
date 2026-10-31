/**
 * @file renderer.h
 * @brief OpenGL モデル資源と描画処理を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_RENDER_RENDERER_H
#define DYNAMIC_WIND_DEMO_RENDER_RENDERER_H

#include "../general/math/matrix4.h"
#include "../general/model/ModelData.h"
#include "camera.h"
#include <memory>

namespace render
{
/**
 * @brief OpenGL に登録された共有モデル。
 * @details 作成、描画、最後の参照の破棄は同じ有効な OpenGL コンテキストで行う。
 */
class Model final
{
public:
    /**
     * @brief CPU モデルを GPU に登録する。
     * @param i_data ローダーから得た静的モデル。
     * @throws std::runtime_error 不正なモデルまたは GPU 登録失敗の場合。
     */
    explicit Model(const loader::ModelData& i_data);
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
private:
    friend class Renderer;
    struct Impl;
    std::unique_ptr<Impl> m_instImpl;
};

/** @brief モデルの照明設定。方向は光が進む方向を表す。 */
struct Lighting
{
    bool enabled = true;
    general::vec::Vector3 direction{0.0F, -1.0F, -1.0F};
};

/**
 * @brief 静的モデルを描画する。
 * @details 有効な OpenGL コンテキストを持つスレッドで使用し、コンテキスト破棄前に破棄する。
 */
class Renderer final
{
public:
    /**
     *  @brief シェーダーを作成する。失敗時は例外を送出する。 
     */
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /**
     * @brief 指定したモデルを Entity のワールド変換で描画する。
     * @param i_model 描画するモデル。
     * @param i_world 同期済みの Entity ワールド行列。
     * @param i_camera カメラ。
     * @param i_aspect 画面の縦横比。
     * @param i_lighting 照明設定。
     */
    void Draw(const Model&                  i_model,
              const general::math::Matrix4& i_world,
              const Camera&                 i_camera,
              float                         i_aspect,
              const Lighting&               i_lighting) const;
private:
    struct                  Impl;           // 描画プログラムを保持する実装詳細をヘッダーから隠す。
    std::unique_ptr<Impl>   m_instImpl;     // 実装オブジェクトを所有し、その寿命を管理する。
};
}
#endif
