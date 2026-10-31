/**
 * @file component_world.h
 * @brief Component、ライフサイクル、および保留要求をまとめて管理する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_WORLD_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_WORLD_H

#include "component_catalog.h"
#include "component_lifecycle.h"
#include "component_read_context.h"
#include "defs/component_world_types.h"
#include "request_queue.h"
#include "system_schedule.h"
#include "../component/transform/transform_traits.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo::ecs
{

/**
 * @brief   登録された Component の格納と、Entity ごとのライフサイクルを管理する。
 * @tparam  Components 使用する Component 型。LocalTransform を含めること。
 * @details
 * ComponentTraits は各型の識別子・依存先・優先度・Initializer を定義する。
 * 初期化処理は自身の Component を変更でき、コンテキストを通じて他の Component を
 * 読み取れる。依存先のポインター等を通じた変更や、失敗時に戻せない外部への変更は
 * 行わないこと。初期化途中の資源は Component 自身の寿命により解放すること。
 * Component への参照は、要求の反映や Entity の削除をまたいで保持しないこと。
 * 本クラスへのアクセスは単一スレッドから行うこと。
 */
template<Component... Components>
class ComponentWorld final
{
    /** @brief Component 初期化の実行段階。 */
    enum class InitializationPhase
    {
        PreInitialize,
        Initialize,
        PostInitialize
    };

    using Transform         =   component::transform::LocalTransform;       // ローカル座標系
    using TransformSystem   =   component::transform::TransformSystem;      // ワールド変換を管理するシステム
    using Catalog           =   ComponentCatalog<Components...>;            // Component のカタログ
    using Staged            =   std::tuple<std::optional<Components>...>;   // 追加など保留中の Component

    // Component カタログ登録確認
    template<typename T>
    static constexpr bool Contains = (std::same_as<T, Components> || ...);

    // Transform Component の存在確認
    static_assert(Contains<Transform>, "Transform Component を登録してください。");
    // Component カタログの妥当性確認
    static_assert(detail::ValidComponentCatalog<Components...>(),
                  "Component の識別子または依存関係が不正です。");
    // Component は例外を送出しないムーブ構築・ムーブ代入・破棄が必要。
    static_assert((std::is_nothrow_move_constructible_v<Components> && ...),
                  "Component は例外を送出しないムーブ構築が必要です。");
    // Component は例外を送出しないムーブ代入が必要。
    static_assert((std::is_nothrow_move_assignable_v<Components> && ...),
                  "Component は例外を送出しないムーブ代入が必要です。");
    // Component は例外を送出しない破棄が必要。
    static_assert((std::is_nothrow_destructible_v<Components> && ...),
                  "Component の破棄処理は例外を送出できません。");

    template<typename T>
    using Stored = detail::WorldStored<T>;      // Entity に対応する Component の格納情報
    using Record = detail::WorldRecord;         // Entity ごとの初期化順序情報

public:
    /** @brief 初期化・終了処理から Component を読み取るためのコンテキスト。 */
    using ReadContext = detail::ComponentReadContext<Components...>;

private:

    // Component の初期化・終了処理の妥当性確認
    static_assert((ComponentInitializer<typename ComponentTraits<Components>::Initializer,
                                        Components, ReadContext> && ...),
                  "Component の初期化・終了処理の定義が不正です。");

public:

    // ========================================================
    // コンストラクタ・デストラクタ（コピー禁止, ムーブ禁止）
    // ========================================================
    /**
     * @brief 空の World を作る。 
     */
    ComponentWorld() : m_instTransforms(m_instEntities) {}
    ComponentWorld(const ComponentWorld&) = delete;
    ComponentWorld& operator=(const ComponentWorld&) = delete;
    ComponentWorld(ComponentWorld&&) = delete;
    ComponentWorld& operator=(ComponentWorld&&) = delete;

    /** 
     * @brief 残った Entity を子から親の順に終了する。 */
    ~ComponentWorld() noexcept
    {
        m_bShutdown = true;
        TerminateAll();
    }


    // ========================================================
    // Entity と Component の生成・追加・削除要求
    // ========================================================
    /**
     * @brief   Entity と Component の生成を次の更新前に要求する。
     * @tparam  T 作成する Component 型。
     * @param   i_components 初期値となる Component。
     * @return  生成結果を参照する要求 ID。
     */
    template<typename... T> requires (Contains<std::decay_t<T>> && ...)
    RequestId RequestCreate(T&&... i_components)
    {
        auto values = std::tuple<std::decay_t<T>...>(std::forward<T>(i_components)...);
        return m_instRequests.Submit(RequestTiming::BeforeUpdate,
            [this, values = std::move(values)]() mutable
            {
                return Create(std::move(values));
            });
    }

    /**
     * @brief 既存 Entity に Component を追加する要求を出す。
     * @tparam T 追加する Component 型。
     * @param i_entity 対象。
     * @param i_components 追加する Component の初期値。
     * @return 処理結果を参照する要求 ID。
     */
    template<typename... T> requires (Contains<std::decay_t<T>> && ...)
    RequestId RequestAdd(const Entity& i_entity, T&&... i_components)
    {
        auto values = std::tuple<std::decay_t<T>...>(std::forward<T>(i_components)...);
        return m_instRequests.Submit(RequestTiming::BeforeUpdate,
            [this, i_entity, values = std::move(values)]() mutable
            {
                return Add(i_entity, std::move(values));
            });
    }

    /**
     * @brief Component の単独削除を次の更新前に要求する。
     * @tparam T 削除する Component 型。
     * @param i_entity 対象。
     * @return 処理結果を参照する要求 ID。
     */
    template<typename T> requires Contains<T>
    RequestId RequestRemove(const Entity& i_entity)
    {
        return m_instRequests.Submit(RequestTiming::BeforeUpdate, [this, i_entity]
        {
            return Remove<T>(i_entity);
        });
    }

    /**
     * @brief Transform の親変更を次の更新前に要求する。
     * @param i_entity 対象。
     * @param i_parent 新しい親。解除する場合は std::nullopt。
     * @return 処理結果を参照する要求 ID。
     */
    RequestId RequestParent(const Entity& i_entity, std::optional<Entity> i_parent)
    {
        return m_instRequests.Submit(RequestTiming::BeforeUpdate,
            [this, i_entity, parent = std::move(i_parent)]
            {
                try
                {
                    FindRecord(i_entity);
                    if (parent) { FindRecord(*parent); }
                    m_instTransforms.SetParent(i_entity, parent);
                    return RequestOutcome::Success();
                }
                catch (const std::invalid_argument& i_error)
                {
                    return RequestOutcome::Failure(i_error.what());
                }
            });
    }

    /**
     * @brief Entity と子孫の削除を全更新後に要求する。
     * @param i_entity 対象。
     * @return 処理結果を参照する要求 ID。
     */
    RequestId RequestDestroy(const Entity& i_entity)
    {
        return m_instRequests.Submit(RequestTiming::AfterUpdate, [this, i_entity]
        {
            if (!IsAlive(i_entity)) { return RequestOutcome::Failure("削除する Entity が無効です。"); }
            DestroySubtree(i_entity);
            return RequestOutcome::Success();
        });
    }

    
    // ========================================================
    // System の登録と更新
    // ========================================================
    /**
     * @brief   System を更新段階と優先度を指定して登録する。
     * @param   i_phase     実行する段階（前更新, 更新, 後更新）。
     * @param   i_priority  大きいほど先に実行する優先度。（同じ優先度の場合は登録順）
     * @param   i_system    経過秒数を受け取る処理。
     */
    void AddSystem(UpdatePhase i_phase, int i_priority, std::function<void(float)> i_system)
    {
        EnsureRunning();
        m_instSchedule.Add(i_phase, i_priority, std::move(i_system));
    }

    /**
     * @brief   更新前の要求、3 段階の System、更新後の削除要求を順に実行する。
     * @param   i_deltaSeconds 有限かつ 0 以上の経過秒数。
     * @throws  std::logic_error 終了後または再入した場合。
     */
    void Update(float i_deltaSeconds)
    {
        EnsureRunning();
        if (!std::isfinite(i_deltaSeconds) || i_deltaSeconds < 0.0F)
        {
            throw std::invalid_argument("経過時間は有限かつ 0 以上で指定してください。");
        }

        // 更新は再入禁止
        if (m_bUpdating || m_uIterationDepth != 0)
        {
            throw std::logic_error("World の更新を再入できません。");
        }
        struct Guard
        {
            bool& updating;
            ~Guard() { updating = false; }
        };
        m_bUpdating = true;
        const auto guard = Guard{ m_bUpdating };

        // 前更新, 更新, 後更新の順に System を実行
        m_instRequests.Process(RequestTiming::BeforeUpdate);
        m_instSchedule.Run(i_deltaSeconds);
        m_instRequests.Process(RequestTiming::AfterUpdate);
    }

    /**
     * @brief 保留要求を中止し、全 Entity を子から親の順に終了する。
     * @throws std::logic_error 更新中に呼び出した場合。
     */
    void Shutdown()
    {
        if (m_bUpdating || m_uIterationDepth != 0)
        {
            throw std::logic_error("World の処理中は終了できません。");
        }

        // もう死んでいる
        if (m_bShutdown) { return; }
        m_instRequests.Shutdown();
        m_bShutdown = true;

        // 全 Entityを終了する前に保留要求を中止しておく
        TerminateAll();
    }


    // ========================================================
    // 要求の結果取得
    // ========================================================
    /**
     * @brief 要求の現在の結果を返す。
     * @param i_request 要求 ID。
     * @return 処理待ち・成功・失敗・中止の結果。
     * @throws std::invalid_argument 要求 ID が無効な場合。
     */
    RequestResult GetResult(RequestId i_request) const { return m_instRequests.GetResult(i_request); }


    /**
     * @brief 完了した要求の結果を受け取り、保持状態から取り除く。
     * @param i_request 要求 ID。
     * @return 完了した要求の結果。
     * @throws std::invalid_argument 要求 ID が無効な場合。
     * @throws std::logic_error 処理待ちの場合。
     */
    RequestResult TakeResult(RequestId i_request) { return m_instRequests.TakeResult(i_request); }

    
    // ========================================================
    // Entity関連
    // ========================================================
    /**
     * @brief Entity が公開済みで現在有効かを返す。
     * @param i_entity 検査する Entity。
     * @return 公開済みかつ有効な場合は true。
     */
    bool IsAlive(const Entity& i_entity) const noexcept
    {
        return  m_instEntities.IsAlive(i_entity) &&
                std::any_of(m_instRecords.begin(), m_instRecords.end(),[&i_entity](const Record& i_record) { 
                    return i_record.entity == i_entity;
                });
    }

    /**
     * @brief 公開済みの Entity の Component を読み取る。
     * @tparam T Component 型。
     * @param i_entity 対象。
     * @return Component の読み取り専用参照。
     * @throws std::invalid_argument Entity または Component が無効な場合。
     */
    template<typename T> requires Contains<T>
    const T& Get(const Entity& i_entity) const
    {
        FindRecord(i_entity);
        return FindStored<T>(i_entity).value;
    }


    // ========================================================
    // Component関連
    // ========================================================
    /**
     * @brief   Transform 以外の Component を種類ごとに処理する。
     * @tparam  T 処理する Component 型。
     * @param   i_function Entity と Component への参照を受け取る処理。
     * @details 構造変更は必ず要求として登録すること。
     */
    template<typename T, typename Function>
        requires (Contains<T> && !std::same_as<T, Transform>)
    void ForEach(Function&& i_function)
    {
        EnsureRunning();
        struct Guard
        {
            std::size_t& depth;
            ~Guard() { --depth; }
        };
        ++m_uIterationDepth;
        const auto guard = Guard{ m_uIterationDepth };
        for (auto& stored : Pool<T>()) { i_function(stored.entity, stored.value); }
    }

    /**
     * @brief Component を種類ごとに読み取る。Transform にも使用できる。
     * @tparam T 読み取る Component 型。
     * @param i_function Entity と Component の定数参照を受け取る処理。
     * @details 構造変更は必ず要求として登録すること。
     */
    template<typename T, typename Function> requires Contains<T>
    void ForEachRead(Function&& i_function)
    {
        EnsureRunning();
        struct Guard
        {
            std::size_t& depth;
            ~Guard() { --depth; }
        };
        // 反復してないのでイテレーションは1だけ増やす。
        ++m_uIterationDepth;
        const auto guard = Guard{ m_uIterationDepth };

        // Entity ごとに Component を処理する
        for (const auto& stored : Pool<T>()) { i_function(stored.entity, stored.value); }
    }


    // ========================================================
    // 座標操作
    // ========================================================
    /**
     * @brief Transform のローカル姿勢を設定する。
     * @param i_entity 対象。
     * @param i_transform 新しい姿勢。
     */
    void SetTransform(const Entity& i_entity, const Transform& i_transform)
    {
        EnsureRunning();

        // Entity のコンポーネントが対応するレコードを取得できるかで存在チェック（なければ例外）
        FindRecord(i_entity);

        // 座標を操作
        auto& value = FindStored<Transform>(i_entity).value;
        m_instTransforms.SetLocal(i_entity, i_transform);
        value = i_transform;
    }

    /** 
     *  @brief 未同期のワールド変換を明示的に同期する。 
     */
    void SynchronizeTransforms() 
    { 
        EnsureRunning();
        m_instTransforms.Synchronize(); 
    }
    
    /**
     *  @brief 読み取り専用の Transform 処理を参照する。 
     */
    const TransformSystem& Transforms() const noexcept 
    {
        return m_instTransforms;
    }

private:

    /** 
     * @brief   更新処理が動き続けていることを確認する。
     * @details 終了済みの場合は例外を投げる。
     */
    void EnsureRunning() const
    {
        if (m_bShutdown) { throw std::logic_error("終了済みの World は使用できません。"); }
    }


    // ========================================================
    // 管理物処理・検索取得・確認・追加・削除
    // ========================================================
    /** 
     * @brief   Entity のレコードを取得する。
     * @details 存在しない場合は例外を投げる。
     * @param   i_entity 対象の Entity。
     * @return  対応する Record への参照。
     */
    Record& FindRecord(const Entity& i_entity)
    {
        // Entity が有効かどうかを確認する。
        if (!m_instEntities.IsAlive(i_entity)) 
        {
            throw std::invalid_argument("Entity が無効です。"); 
        }

        // マッチするEntityがいるか確認する。
        const auto found = std::find_if(
            m_instRecords.begin(),
            m_instRecords.end(),
            [&i_entity](const Record& i_record){ 
                return i_record.entity == i_entity; 
            });
        if (found == m_instRecords.end()) { throw std::invalid_argument("Entity は公開されていません。"); }
        return *found;
    }

    /** 
     * @brief   Entity のレコードを取得する（読み取り専用）。
     * @details 存在しない場合は例外を投げる。
     * @param   i_entity 対象の Entity。
     * @return  対応する Record への参照。
     */
    const Record& FindRecord(const Entity& i_entity) const
    {
        // Entity が有効かどうかを確認する。
        if (!m_instEntities.IsAlive(i_entity)) 
        {
            throw std::invalid_argument("Entity が無効です。"); 
        }

        // マッチするEntityがいるか確認する。
        const auto found = std::find_if(
            m_instRecords.begin(),
            m_instRecords.end(),
            [&i_entity](const Record& i_record) {
                return i_record.entity == i_entity;
            });
        if (found == m_instRecords.end()) { throw std::invalid_argument("Entity は公開されていません。"); }
        return *found;
    }

    /**
     * @brief   指定された型コンポーネントを持つEntityのプールを取得する
     * @tparam  T コンポーネントの型
     * @return  対応するコンポーネントプールへの参照
     */
    template<typename T>
    std::vector<Stored<T>>& Pool() 
    { 
        return std::get<std::vector<Stored<T>>>(m_instPools);
    }

    /**
     * @brief   指定された型コンポーネントを持つEntityのプールを取得する(読み取り専用)
     * @tparam  T コンポーネントの型
     * @return  対応するコンポーネントプールへの参照
     */
    template<typename T>
    const std::vector<Stored<T>>& Pool() const { return std::get<std::vector<Stored<T>>>(m_instPools); }

    /**
     * @brief   指定された型コンポーネントを持つEntityのストアを取得する
     * @tparam  T コンポーネントの型
     * @param   i_entity 対象の Entity
     * @return  対応する Entitiyのコンポーネントへの参照
     */
    template<typename T>
    Stored<T>& FindStored(const Entity& i_entity)
    {
        auto& pool = Pool<T>();
        const auto found = std::find_if(
            pool.begin(),
            pool.end(),
            [&i_entity](const Stored<T>& i_stored) 
            { 
                return i_stored.entity == i_entity;
            });
        if (found == pool.end()) { throw std::invalid_argument("Component が存在しません。"); }
        return *found;
    }

    /**
     * @brief   指定された型コンポーネントを持つEntityのストアを取得する(読み取り専用)
     * @tparam  T コンポーネントの型
     * @param   i_entity 対象の Entity
     * @return  対応する Entitiyのコンポーネントへの参照
     */
    template<typename T>
    const Stored<T>& FindStored(const Entity& i_entity) const
    {
        const auto& pool = Pool<T>();
        const auto found = std::find_if(pool.begin(), pool.end(),
            [&i_entity](const Stored<T>& i_stored) { return i_stored.entity == i_entity; });
        if (found == pool.end()) { throw std::invalid_argument("Component が存在しません。"); }
        return *found;
    }

    /**
     * @brief   指定された型コンポーネントを持つEntityのストアが存在するかを確認する(読み取り専用)
     * @tparam  T コンポーネントの型
     * @param   i_entity 対象の Entity
     * @return  存在する場合は true、存在しない場合は false
     */
    template<typename T>
    bool HasStored(const Entity& i_entity) const noexcept
    {
        const auto& pool = Pool<T>();
        return std::any_of(
            pool.begin(),
            pool.end(),
            [&i_entity](const Stored<T>& i_stored) {
                return i_stored.entity == i_entity;
            });
    }

    /**
     * @brief   指定された型コンポーネントを持つEntityのStagedを作成する
     * @tparam  T コンポーネントの型
     * @param   i_values コンポーネントの初期値のタプル
     * @return  作成された Staged(std::tuple<std::optional<Components>...>) オブジェクト
     */
    template<typename... T>
    static Staged MakeStaged(std::tuple<T...>&& i_values)
    {
        auto staged = Staged{};
        // 引数取得したコンポーネント群を Staged に格納する
        std::apply([&staged](auto&&... i_value) {
            (std::get<std::optional<std::decay_t<decltype(i_value)>>>(staged)
                .emplace(std::forward<decltype(i_value)>(i_value)), ...);
            },
            std::move(i_values)
        );
        return staged;
    }

    /**
     * @brief   指定された型コンポーネントの種類を取得する
     * @tparam  T コンポーネントの型
     * @param   i_tuple コンポーネントのタプル
     * @return  コンポーネントの種類列挙値の配列
     */
    template<typename... T>
    static std::array<ComponentKind, sizeof...(T)> Kinds(const std::tuple<T...>&)
    {
        return { ComponentTraits<T>::kind... };
    }

    /**
     * @brief   指定された初期化段階の処理を実行する
     * @param   i_phase 初期化段階。
     * @param   i_kind コンポーネントの種類
     * @param   io_staged ステージングされたコンポーネント群
     * @param   i_context 読み取り専用のコンテキスト
     * @return  初期化結果
     */
    InitializationResult RunPhase(const InitializationPhase i_phase,
                                  const ComponentKind       i_kind,
                                  Staged&                   io_staged,
                                  const ReadContext&        i_context)
    {
        // それぞれのコンポーネントの初期化の成否の格納先
        auto result = std::optional<InitializationResult>{};

        // 指定されたコンポーネントの初期化処理を呼び出すラムダ関数を定義
        const auto invoke = [&]<typename T>()
        {
            // 指定されたコンポーネントか確認
            if (ComponentTraits<T>::kind != i_kind) { return; }

            // ステージングされたコンポーネントを取得
            auto& item = std::get<std::optional<T>>(io_staged);
            if (!item) 
            { 
                throw std::logic_error("初期化する Component がありません。"); 
            }

            // コンポーネントの初期化機定義型を取得
            using Initializer = typename ComponentTraits<T>::Initializer;
            switch (i_phase)
            {
            case InitializationPhase::PreInitialize:
                // 前初期化 フェーズの処理
                result.emplace(Initializer::PreInitialize(*item, i_context));
                break;
            case InitializationPhase::Initialize:
                // 初期化 フェーズの処理
                result.emplace(Initializer::Initialize(*item, i_context));
                break;
            case InitializationPhase::PostInitialize:
                // 後初期化 フェーズの処理
                result.emplace(Initializer::PostInitialize(*item, i_context));
                break;
            default: throw std::logic_error("初期化段階が不正です。");
            }
        };

        // ラムダ関数をすべてのコンポーネントに対して呼び出す（Componentsはクラスの頭で定義している）
        (invoke.template operator()<Components>(), ...);
        if (!result) { throw std::logic_error("Component の初期化処理がありません。"); }
        return std::move(*result);
    }

    /**
     * @brief コンポーネントの初期化を行う
     * 
     * @param   io_staged   ステージングされたコンポーネント
     * @param   i_entity    対象のエンティティ
     * @param   i_order     初期化順序
     * @return  初期化結果
     */
    InitializationResult Initialize(Staged&                             io_staged,
                                    const Entity&                       i_entity,
                                    const std::vector<ComponentKind>&   i_order)
    {
        // コンポーネントアクセス用のコンテキストを作成
        const auto context = ReadContext(*this, i_entity, &io_staged);

        // 初期化フェーズを定義
        constexpr auto phases = std::array
        {
            InitializationPhase::PreInitialize,
            InitializationPhase::Initialize,
            InitializationPhase::PostInitialize
        };

        // 各初期化フェーズを順に実行
        for (const auto phase : phases)
        {
            // コンテナに積まれた順序で初期化を実行
            for (const auto kind : i_order)
            {
                // 指定されたステージングコンポーネントに対して初期化を実行
                auto result = RunPhase(phase, kind, io_staged, context);
                
                // 初期化順序に依存するものもある可能性があるため、失敗した場合は即座に返す
                if (!result.Succeeded()) 
                { 
                    return result; 
                }
            }
        }
        return InitializationResult::Success();
    }

    /**
     * @brief コンテナの容量を追加
     * 
     * @param   i_staged    ステージングされたコンポーネント
     * @tparam  T           プールの要素型
     */
    template<typename T>
    static void ReserveAppend(std::vector<T>& io_values)
    {
        // 現在の容量が十分であれば何もしない
        if (io_values.size() < io_values.capacity()) { return; }

        // 現在の容量が最大値に達していれば例外を投げる
        const auto maximum = io_values.max_size();
        if (io_values.size() == maximum) { throw std::length_error("Component の格納数が上限を超えました。"); }

        // 新しい容量を計算して確保
        const auto capacity =   io_values.capacity();
        const auto growth   =   std::min(capacity, maximum - capacity);
        const auto target   =   std::max(io_values.size() + 1, capacity + growth);
        io_values.reserve(target);
    }

    /**
     * @brief プールの容量を確保
     * 
     * @param   i_staged    ステージングされたコンポーネント
     */
    void ReservePools(const Staged& i_staged)
    {
        const auto reserve = [&]<typename T>()
        {
            if (std::get<std::optional<T>>(i_staged))
            {
                auto& pool = Pool<T>();
                ReserveAppend(pool);
            }
        };
        (reserve.template operator()<Components>(), ...);
    }

    /**
     * @brief ステージングされたコンポーネントにEntityと紐づいたコンポーネントをプールに追加
     * 
     * @param   i_entity    エンティティ
     * @param   io_staged   ステージングされたコンポーネント
     */
    void CommitPools(const Entity& i_entity, Staged& io_staged) noexcept
    {
        const auto commit = [&]<typename T>()
        {
            auto& item = std::get<std::optional<T>>(io_staged);
            if (item) 
            { 
                Pool<T>().push_back(Stored<T>{ i_entity, std::move(*item) }); 
            }
        };
        (commit.template operator()<Components>(), ...);
    }

    /**
     * @brief エンティティを作成してコンポーネントを追加
     * 
     * @tparam  T           コンポーネントの型
     * @param   i_values    コンポーネントの値
     * @return  RequestOutcome 作成結果
     */
    template<typename... T>
    RequestOutcome Create(std::tuple<T...>&& i_values)
    {
        try
        {
            // コンポーネントの種類を取得
            const auto kinds = Kinds(i_values);

            // 初期化順序にコンポーネント種別をソート
            auto order = Catalog::InitializationOrder(kinds, {});

            // optionalなコンポーネント型tupleを作成
            auto staged = MakeStaged(std::move(i_values));

            // エンティティを作成
            const auto entity = m_instEntities.Create();
            struct Guard
            {
                ComponentWorld& world;                  // 所属するComponentWorld
                Entity          entity;                 // 対応するエンティティ
                bool            attached    =   false;  // Transform コンポーネントが追加されたか
                bool            committed   =   false;  // プールにコミットされたか
                ~Guard()
                {
                    // コミットされていなければエンティティを破棄
                    if (!committed)
                    {
                        if (attached) 
                        {
                            world.m_instTransforms.Remove(entity);
                        }
                        world.m_instEntities.Destroy(entity);
                    }
                }
            };
            auto guard = Guard{ *this, entity };

            // Entity を初期化
            const auto initialized = Initialize(staged, entity, order);
            if (!initialized.Succeeded()) 
            {
                return RequestOutcome::Failure(initialized.Message());
            }

            // コンポーネントのプールの確保とEntityへの初期化順序の登録
            ReservePools(staged);
            ReserveAppend(m_instRecords);

            // Transform コンポーネントの追加
            auto& transform = std::get<std::optional<Transform>>(staged);
            if (transform)
            {
                m_instTransforms.Add(entity, *transform);
                guard.attached = true;
            }

            // Entity のコンポーネントをプールへコミット
            CommitPools(entity, staged);
            m_instRecords.push_back(Record{ entity, std::move(order) });
            guard.committed = true;

            // 成功した場合はエンティティの追加が成功したことを示す
            return RequestOutcome::Success(entity);
        }

        // 例外が発生した場合は失敗理由を登録する
        catch (const std::invalid_argument& i_error) { return RequestOutcome::Failure(i_error.what()); }
        catch (const std::overflow_error& i_error) { return RequestOutcome::Failure(i_error.what()); }
    }

    /**
     * @brief   既存のエンティティにコンポーネントを追加する
     * @tparam  T   追加するコンポーネントの型
     * @param   i_entity    追加対象のエンティティ
     * @param   i_values    追加するコンポーネントの値のタプル
     * @return  RequestOutcome  成功した場合は成功結果、失敗した場合は失敗理由を含む
     */
    template<typename... T>
    RequestOutcome Add(const Entity& i_entity, std::tuple<T...>&& i_values)
    {
        try
        {
            // 既存のエンティティのレコードを取得
            auto& record = FindRecord(i_entity);

            // 追加するコンポーネントの種類配列を取得してコンポーネントの初期化順序にソート
            const auto kinds = Kinds(i_values);
            auto order = Catalog::InitializationOrder(kinds, record.order);

            // コンポーネントのステージリストを作成して初期化
            auto staged = MakeStaged(std::move(i_values));
            const auto initialized = Initialize(staged, i_entity, order);
            if (!initialized.Succeeded()) 
            {
                return RequestOutcome::Failure(initialized.Message());
            }

            // プールの容量をと追加分の余剰確認
            ReservePools(staged);
            if (order.size() > record.order.max_size() - record.order.size())
            {
                throw std::length_error("Component の登録数が上限を超えました。");
            }
            record.order.reserve(record.order.size() + order.size());

            // Transform コンポーネントが存在する場合はインスタンス管理に追加
            auto& transform = std::get<std::optional<Transform>>(staged);
            if (transform) 
            {
                m_instTransforms.Add(i_entity, *transform);
            }

            // Entity をコンポーネントワールドに追加
            CommitPools(i_entity, staged);

            // コンポーネントの初期化順序の末尾に追加物種別を挿入
            record.order.insert(record.order.end(), order.begin(), order.end());
            return RequestOutcome::Success();
        }
        catch (const std::invalid_argument& i_error) { return RequestOutcome::Failure(i_error.what()); }
        catch (const std::overflow_error& i_error) { return RequestOutcome::Failure(i_error.what()); }
    }

    /**
     * @brief   指定したエンティティのコンポーネントを終了する
     * @param   i_entity    終了対象のエンティティ
     * @param   i_kind      終了対象のコンポーネントの種類
     */
    void TerminateComponent(const Entity& i_entity, const ComponentKind i_kind) noexcept
    {
        // エンティティからコンポーネントを読む
        const auto context = ReadContext(*this, i_entity, nullptr);
        const auto terminate = [&]<typename T>()
        {
            // 修飾情報にコンポーネント種別がない場合は処理しない
            if (ComponentTraits<T>::kind != i_kind) { return; }

            // 指定した種別のコンポーネントプールを取得して該当するEntityを検索
            auto& pool = Pool<T>();
            const auto found = std::find_if(
                pool.begin(),
                pool.end(),
                [&i_entity](const Stored<T>& i_value) {
                    return i_value.entity == i_entity;
                }
            );
            // 修飾型内の終了処理を呼び出す
            using Initializer = typename ComponentTraits<T>::Initializer;
            Initializer::Terminate(found->value, context);

            // Transform コンポーネントの場合は独自のインスタンス管理から削除
            if constexpr (std::same_as<T, Transform>)
            {
                // 親子関係を解除してから削除
                if (m_instTransforms.GetParent(i_entity))
                {
                    m_instTransforms.SetParent(i_entity, std::nullopt);
                }
                m_instTransforms.Remove(i_entity);
            }

            // プールから該当する Entity を削除
            pool.erase(found);
        };
        (terminate.template operator()<Components>(), ...);
    }

    /**
     * @brief   指定したエンティティが子を持つか確認する
     * @param   i_entity    確認対象のエンティティ
     * @return  子を持つ場合は true、それ以外は false
     */
    bool HasChild(const Entity& i_entity) const
    {
        // 初期化の順序レコードを順に確認して、Transform コンポーネントの親子関係を持つか
        for (const auto& record : m_instRecords)
        {
            if (HasStored<Transform>(record.entity))
            {
                const auto parent = m_instTransforms.GetParent(record.entity);
                if (parent && *parent == i_entity)
                {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief   指定したエンティティからコンポーネントを削除する
     * @tparam  T           削除するコンポーネントの型
     * @param   i_entity    削除対象のエンティティ
     * @return  削除が成功した場合は RequestOutcome::Success、それ以外は RequestOutcome::Failure
     */
    template<typename T>
    RequestOutcome Remove(const Entity& i_entity)
    {
        try
        {
            // 指定されたエンティティのレコードを取得
            auto& record = FindRecord(i_entity);

            // コンポーネントの種類を取得
            constexpr auto kind = ComponentTraits<T>::kind;

            // 依存関係がないか確認
            Catalog::ValidateRemoval(kind, record.order);
            if constexpr (std::same_as<T, Transform>)
            {
                // Transform コンポーネントの場合は親子関係を確認
                if (m_instTransforms.GetParent(i_entity) || HasChild(i_entity))
                {
                    return RequestOutcome::Failure("親子関係のある Transform は単独で削除できません。");
                }
            }

            // コンポーネントを削除
            TerminateComponent(i_entity, kind);

            // 初期化順序から削除したコンポーネントの種類を削除
            record.order.erase(std::find(record.order.begin(), record.order.end(), kind));
            return RequestOutcome::Success();
        }
        catch (const std::invalid_argument& i_error) { return RequestOutcome::Failure(i_error.what()); }
    }

    /**
     * @brief   指定したエンティティを破棄する
     * @param   i_entity    破棄対象のエンティティ
     */
    void DestroyLeaf(const Entity& i_entity) noexcept
    {
        // 指定されたエンティティのコンポーネントレコードを取得
        auto& record = FindRecord(i_entity);
        for (auto it = record.order.rbegin(); it != record.order.rend(); ++it)
        {
            // 初期化順とは逆順にコンポーネントストアを破棄する
            TerminateComponent(i_entity, *it);
        }

        // 該当するレコードとエンティティを削除
        const auto found = std::find_if(
            m_instRecords.begin(),
            m_instRecords.end(),
            [&i_entity](const Record& i_record) {
                return i_record.entity == i_entity;
            }
        );
        m_instRecords.erase(found);
        m_instEntities.Destroy(i_entity);
    }

    /**
     * @brief   指定したエンティティをルートとして破棄する
     * @param   i_entity    破棄対象のエンティティ
     */
    void DestroySubtree(const Entity& i_entity)
    {
        auto entities = std::vector<Entity>{ i_entity };
        for (auto index = std::size_t{}; index < entities.size(); ++index)
        {
            // Transform コンポーネントの親子関係を辿り、親エンティティを収集する
            for (const auto& record : m_instRecords)
            {
                if (!HasStored<Transform>(record.entity)) { continue; }
                const auto parent = m_instTransforms.GetParent(record.entity);
                if (parent && *parent == entities[index])
                {
                    entities.push_back(record.entity);
                }
            }
        }

        // 自身から子から順に依存関係を維持しながら破棄していく
        for (auto it = entities.rbegin(); it != entities.rend(); ++it) { DestroyLeaf(*it); }
    }

    /**
     * @brief   すべてのエンティティを破棄する
     */
    void TerminateAll() noexcept
    {
        while (!m_instRecords.empty())
        {
            auto found = std::find_if(
                m_instRecords.begin(),
                m_instRecords.end(),
                [this](const Record& i_record)
                {
                    return !HasChild(i_record.entity);
                }
            );
            // レコードに残っているのに子を持たないエンティティが見つからない場合は異常終了
            if (found == m_instRecords.end()) { std::terminate(); }

            // 子を持たないエンティティが見つかったので、そのエンティティを破棄する
            const auto entity = found->entity;

            // 子から親の順に破棄していく
            DestroyLeaf(entity);
        }
    }

    EntityRegistry                                  m_instEntities;     // Entity 識別子の発行と有効性を管理
    TransformSystem                                 m_instTransforms;   // Transform の親子関係とワールド変換を管理
    SystemSchedule                                  m_instSchedule;     // System の登録と実行順を管理
    RequestQueue                                    m_instRequests;     // 処理タイミング別の保留要求を管理
    std::tuple<std::vector<Stored<Components>>...>  m_instPools;        // Component 型ごとの格納領域
    std::vector<Record>                             m_instRecords;      // 公開 Entity と初期化順序の記録
    bool        m_bUpdating         =   false;                          // 更新処理中かどうか
    bool        m_bShutdown         =   false;                          // 終了処理済みかどうか
    std::size_t m_uIterationDepth   =   0;                              // Component 反復処理のネスト数
};
}
#endif
