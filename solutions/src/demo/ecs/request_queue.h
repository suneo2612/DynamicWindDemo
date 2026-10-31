/**
 * @file request_queue.h
 * @brief 構造変更要求の保留と結果の管理を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_REQUEST_QUEUE_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_REQUEST_QUEUE_H

#include "defs/request_queue_defs.h"
#include <concepts>
#include <deque>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace demo::ecs
{

/**
 * @brief 要求処理の明示的な成功または失敗。 
 */
class RequestOutcome final
{
public:
    /**
     * @brief 成功を返す。
     * @param i_entity 生成に成功した Entity。生成以外では省略する。
     * @return 成功結果。
     */
    static RequestOutcome Success(std::optional<Entity> i_entity = std::nullopt);

    /**
     * @brief 理由を伴う失敗を返す。
     * @param i_message 失敗理由。空文字列は指定できない。
     * @return 失敗結果。
     * @throws std::invalid_argument 理由が空の場合。
     */
    static RequestOutcome Failure(std::string i_message);

private:
    friend class RequestQueue;      // RequestQueue へのアクセス権を付与

    // queue からのアクセスを許可するためのコンストラクタとメンバー変数
    RequestOutcome(RequestStatus i_status, std::optional<Entity> i_entity, std::string i_message);
    
    RequestStatus           m_instStatus;   // 要求の状態
    std::optional<Entity>   m_instEntity;   // 生成に成功した Entity
    std::string             m_instMessage;  // 失敗理由
};

/**
 * @brief 同じ区切りの要求を受付順に実行し、結果を明示的な取得まで保持する。
 * @details
 * 処理中に追加された同じ区切りの要求は、次の同じ区切りで実行する。
 * 要求の操作が例外を送出した場合は、その要求を失敗として記録し、例外を再送出する。
 * 操作途中のデータや資源の巻き戻しは、要求の操作自身が保証する必要がある。
 */
class RequestQueue final
{
public:
    /** @brief 空の要求キューを作る。 */
    RequestQueue() = default;
    RequestQueue(const RequestQueue&) = delete;
    RequestQueue& operator=(const RequestQueue&) = delete;
    RequestQueue(RequestQueue&&) = delete;
    RequestQueue& operator=(RequestQueue&&) = delete;

    /**
     * @brief 実行する操作を受付順に保留する。
     * @tparam Action 引数なしで RequestOutcome を返す操作。
     * @param i_timing 要求を反映する区切り。
     * @param i_action 実行する操作。ムーブ専用の操作も受け付ける。
     * @return 要求結果を参照する識別子。
     * @throws std::logic_error キューが終了済みの場合。
     * @throws std::invalid_argument 反映タイミングが不正な場合。
     * @throws std::overflow_error 識別子を使い切った場合。
     */
    template<typename Action>
        requires std::same_as<std::invoke_result_t<std::decay_t<Action>&>, RequestOutcome>
    RequestId Submit(const RequestTiming i_timing, Action&& i_action)
    {
        return SubmitImpl(i_timing, std::make_unique<Command<std::decay_t<Action>>>(
            std::forward<Action>(i_action)));
    }

    /**
     * @brief 指定した区切りで処理待ちの要求を受付順に実行する。
     * @param i_timing 反映する区切り。
     * @throws std::invalid_argument 反映タイミングが不正な場合。
     * @throws std::logic_error 再入または終了後に実行した場合。
     */
    void Process(RequestTiming i_timing);

    /**
     * @brief 状態を取得する。結果は消費しない。
     * @param i_request 要求識別子。
     * @return 現在の結果。
     * @throws std::invalid_argument 存在しない識別子の場合。
     */
    RequestResult GetResult(RequestId i_request) const;

    /**
     * @brief 完了した要求の結果を取得し、保持状態から取り除く。
     * @param i_request 要求識別子。
     * @return 完了した結果。
     * @throws std::invalid_argument 存在しない識別子の場合。
     * @throws std::logic_error 処理待ちの場合。
     */
    RequestResult TakeResult(RequestId i_request);

    /**
     * @brief 処理待ちの要求を中止し、理由を保持する。
     * @throws std::logic_error 要求処理中に呼び出した場合。
     * @throws std::length_error 中止する要求数がコンテナーの上限を超える場合。
     */
    void Shutdown();

private:

    /**
     * @brief 要求を表す基底クラス。
     * @details 実際の操作は Command テンプレートで実装される。
     */
    struct CommandBase
    {
        virtual ~CommandBase() = default;
        virtual RequestOutcome Execute() = 0;
    };

    /**
     * @brief   要求を表す具体的なクラス。
     * @tparam  Action 実行する操作の型。
     * @details Execute() で操作を実行し、結果を返す。
     */
    template<typename Action>
    struct Command final : CommandBase
    {
        template<typename RequestAction>
        explicit Command(RequestAction&& i_action) 
            : action(std::forward<RequestAction>(i_action))
        {
        }
        RequestOutcome Execute() override { return action(); }
        Action action;      // 要求で実行する操作
    };

    /**
     * @brief 処理待ちの要求を表す構造体。
     */
    struct Pending
    {
        RequestId id;                           // 要求識別子
        std::unique_ptr<CommandBase> command;   // 実行する要求
    };

    /**
     * @brief 要求結果を登録し、コマンドを指定した区切りのキューに追加する。
     * @param i_timing 要求を反映する区切り。
     * @param i_command 実行するコマンド。
     * @return 要求結果を参照する識別子。
     * @throws std::logic_error キューが終了済みの場合。
     * @throws std::invalid_argument 反映タイミングが不正な場合。
     * @throws std::overflow_error 識別子を使い切った場合。
     */
    RequestId SubmitImpl(RequestTiming i_timing, std::unique_ptr<CommandBase> i_command);

    /**
     * @brief 指定された反映タイミングの要求キューを取得する。
     * @param i_timing 反映タイミング。
     * @return 要求キューへの参照。
     * @throws std::invalid_argument 反映タイミングが不正な場合。
     */
    std::deque<Pending>& Queue(RequestTiming i_timing);

    std::deque<Pending> m_instBefore;                               // 区切り前の処理待ち要求
    std::deque<Pending> m_instAfter;                                // 区切り後の処理待ち要求
    std::unordered_map<RequestId, RequestResult> m_instResults;     // 完了した要求の結果
    RequestId m_uNext = 1;                                          // 次に割り当てる要求識別子
    bool m_bProcessing = false;                                     // 現在処理中かどうか
    bool m_bShutdown = false;                                       // シャットダウン済みかどうか
};
}
#endif
