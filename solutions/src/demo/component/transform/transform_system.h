/**
 * @file transform_system.h
 * @brief 姿勢データと親子変換の明示的な同期を提供する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_COMPONENT_TRANSFORM_TRANSFORM_SYSTEM_H
#define DYNAMIC_WIND_DEMO_DEMO_COMPONENT_TRANSFORM_TRANSFORM_SYSTEM_H

#include "../../ecs/entity_registry.h"
#include "../../../general/math/matrix4.h"
#include <optional>
#include <vector>

namespace demo::component::transform
{

/**
 * @brief ローカル座標における姿勢。
 */
struct LocalTransform
{
    general::vec::Vector3 position{};
    general::math::Quaternion rotation{};
    general::vec::Vector3 scale{1.0F, 1.0F, 1.0F};
};

/**
 * @brief 親子の Transform を保持し、明示的な同期時にワールド変換を計算する。
 * @details
 管理元の EntityRegistry はこのインスタンスより長く生存する必要がある。
 * Entity を無効化する前に Transform と親子関係を取り除くこと。
 * ローカル・ワールド方向は四元数の回転から算出し、スケールを含めない。
 * 負スケールや非一様スケールによって、変形後のモデルの軸と一致しない場合がある。
 */
class TransformSystem final
{
public:
    /**
     * @brief Entity の管理元を指定する。
     * @param i_registry この System より長く生存する管理元。
     */
    explicit TransformSystem(const ecs::EntityRegistry& i_registry) noexcept;
    TransformSystem(const TransformSystem&) = delete;
    TransformSystem& operator=(const TransformSystem&) = delete;

    /**
     * @brief 有効な Entity に Transform を追加する。
     * @param i_entity 対象。
     * @param i_transform 初期ローカル姿勢。
     * @throws std::invalid_argument Entity または姿勢が不正な場合。
     * @throws std::logic_error 既に Transform を保持している場合。
     */
    void Add(const ecs::Entity& i_entity, const LocalTransform& i_transform);

    /**
     * @brief 親子関係を持たない Entity の Transform を削除する。
     * @param i_entity 対象。
     * @throws std::logic_error 親子関係が残っている場合。
     * @throws std::invalid_argument Entity または Transform が無効な場合。
     */
    void Remove(const ecs::Entity& i_entity);

    /**
     * @brief ローカル姿勢を設定し、自身と子孫を未同期にする。
     * @param i_entity 対象。
     * @param i_transform 新しい姿勢。
     * @throws std::invalid_argument Entity または姿勢が不正な場合。
     */
    void SetLocal(const ecs::Entity& i_entity, const LocalTransform& i_transform);

    /**
     * @brief ローカル姿勢を維持したまま親を変更する。
     * @param i_entity 子 Entity。
     * @param i_parent 新しい親。親を解除する場合は std::nullopt。
     * @throws std::invalid_argument Entity が無効、または循環が生じる場合。
     */
    void SetParent(const ecs::Entity& i_entity, const std::optional<ecs::Entity>& i_parent);

    /**
     * @brief 現在の Transform 親を取得する。
     * @param i_entity 対象。
     * @return 親 Entity。親がなければ std::nullopt。
     * @throws std::invalid_argument Entity または Transform が無効な場合。
     */
    std::optional<ecs::Entity> GetParent(const ecs::Entity& i_entity) const;

    /**
     * @brief 親から子の順に未同期のワールド変換をまとめて計算する。
     * @throws std::invalid_argument 管理中の Entity が既に無効な場合。
     * @throws std::overflow_error 行列の演算結果が表現可能な範囲外の場合。
     */
    void Synchronize();

    /**
     * @brief ローカル姿勢を値として返す。
     * @param i_entity 対象。
     * @return ローカル姿勢。
     * @throws std::invalid_argument Entity または Transform が無効な場合。
     */
    LocalTransform GetLocal(const ecs::Entity& i_entity) const;
    /**
     * @brief ローカル姿勢における前方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない前方向。
     */
    general::vec::Vector3 GetLocalForward(const ecs::Entity& i_entity) const;
    /**
     * @brief ローカル姿勢における右方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない右方向。
     */
    general::vec::Vector3 GetLocalRight(const ecs::Entity& i_entity) const;
    /**
     * @brief ローカル姿勢における上方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない上方向。
     */
    general::vec::Vector3 GetLocalUp(const ecs::Entity& i_entity) const;

    /**
     * @brief 同期済みのワールド行列を返す。
     * @param i_entity 対象。
     * @return 列優先のワールド変換。
     * @throws std::logic_error 対象が未同期の場合。
     * @throws std::invalid_argument 対象または親 Entity が無効な場合。
     */
    general::math::Matrix4 GetWorldMatrix(const ecs::Entity& i_entity) const;
    /**
     * @brief 同期済みのワールド位置を返す。
     * @param i_entity 対象。
     * @return ワールド座標の位置。
     * @throws std::logic_error 対象が未同期の場合。
     */
    general::vec::Vector3 GetWorldPosition(const ecs::Entity& i_entity) const;
    /**
     * @brief 同期済みのワールド姿勢を返す。
     * @param i_entity 対象。
     * @return 親から子へ合成した、スケールを含まない四元数。
     * @throws std::logic_error 対象が未同期の場合。
     */
    general::math::Quaternion GetWorldRotation(const ecs::Entity& i_entity) const;
    /**
     * @brief 同期済みのワールド姿勢における前方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない前方向。
     * @throws std::logic_error 対象が未同期の場合。
     */
    general::vec::Vector3 GetWorldForward(const ecs::Entity& i_entity) const;
    /**
     * @brief 同期済みのワールド姿勢における右方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない右方向。
     * @throws std::logic_error 対象が未同期の場合。
     */
    general::vec::Vector3 GetWorldRight(const ecs::Entity& i_entity) const;
    /**
     * @brief 同期済みのワールド姿勢における上方向を返す。
     * @param i_entity 対象。
     * @return スケールを含まない上方向。
     * @throws std::logic_error 対象が未同期の場合。
     */
    general::vec::Vector3 GetWorldUp(const ecs::Entity& i_entity) const;

private:

    /**
     * @brief TransformSystem が管理する Entity ごとの姿勢情報。
     * @details
     * ローカル姿勢と親 Entity を保持し、Synchronize で計算したワールド姿勢をキャッシュする。
     * dirty が true の間はワールド姿勢のキャッシュが未同期である。
     */
    struct Entry
    {
        ecs::Entity                 entity;                 // Transform を保持する Entity。
        std::optional<ecs::Entity>  parent;                 // 親 Entity。親がなければ std::nullopt。
        LocalTransform              local;                  // 親を基準とするローカル位置・回転・スケール。
        general::math::Matrix4      world;                  // Synchronize で計算したワールド変換行列。
        general::math::Quaternion   worldRotation;          // 親子の回転を合成したワールド回転。
        bool                        dirty   =   true;       // true の場合、ワールド姿勢の再計算が必要。
    };

    /** 
     *  @brief エントリを検索する。
     *  @param i_entity 対象。
     *  @return エントリのインデックス。
     */
    std::size_t Find(const ecs::Entity& i_entity) const;

    /**
     *  @brief 同期済みのエントリを取得する。
     *  @param i_entity 対象。
     *  @return 同期済みのエントリ。
     *  @throws std::logic_error 対象が未同期の場合。
     */
    const Entry& Synchronized(const ecs::Entity& i_entity) const;
    
    
    /**
     *  @brief 子孫のエントリを更新が必要な状態にする。
     *  @param i_entity 対象。
     */
    void MarkDescendants(const ecs::Entity& i_entity);

    const ecs::EntityRegistry&  m_instRegistry;     // EntityRegistry への参照（本体は外部で管理される）
    std::vector<Entry>          m_instEntries;      // エントリのリスト
};
}
#endif
