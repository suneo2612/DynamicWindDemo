/**
 * @file request_queue.cpp
 * @brief 要求の受付順、反映タイミング、および結果を管理する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "request_queue.h"
#include <limits>
#include <stdexcept>
#include <vector>

namespace demo::ecs
{

/**
 * @brief RequestOutcome のコンストラクタ。
 * @param i_status 要求の状態。
 * @param i_entity 生成に成功した Entity。
 * @param i_message 失敗理由。
 */
RequestOutcome::RequestOutcome(const RequestStatus i_status,
                               std::optional<Entity> i_entity,
                               std::string i_message)
    : m_instStatus(i_status)
    , m_instEntity(std::move(i_entity))
    , m_instMessage(std::move(i_message))
{
}

/**
 * @brief 成功を返す。
 * @param i_entity 生成に成功した Entity。生成以外では省略する。
 * @return 成功結果。
 */
RequestOutcome RequestOutcome::Success(std::optional<Entity> i_entity)
{
    return RequestOutcome(RequestStatus::Succeeded, std::move(i_entity), {});
}

/**
 * @brief 理由を伴う失敗を返す。
 * @param i_message 失敗理由。空文字列は指定できない。
 * @return 失敗結果。
 * @throws std::invalid_argument 理由が空の場合。
 */
RequestOutcome RequestOutcome::Failure(std::string i_message)
{
    if (i_message.empty())
    {
        throw std::invalid_argument("要求の失敗理由を指定してください。");
    }
    return RequestOutcome(RequestStatus::Failed, std::nullopt, std::move(i_message));
}

/**
 * @brief 指定された反映タイミングの要求キューを取得する。
 * @param i_timing 反映タイミング。
 * @return 要求キューへの参照。
 * @throws std::invalid_argument 反映タイミングが不正な場合。
 */
std::deque<RequestQueue::Pending>& RequestQueue::Queue(const RequestTiming i_timing)
{
    switch (i_timing)
    {
    case RequestTiming::BeforeUpdate: return m_instBefore;
    case RequestTiming::AfterUpdate: return m_instAfter;
    default: throw std::invalid_argument("要求の反映タイミングが不正です。");
    }
}

/**
 * @brief 要求結果を登録し、コマンドを指定した区切りのキューに追加する。
 * @param i_timing 要求を反映する区切り。
 * @param i_command 実行するコマンド。
 * @return 要求結果を参照する識別子。
 * @throws std::logic_error キューが終了済みの場合。
 * @throws std::invalid_argument 反映タイミングが不正な場合。
 * @throws std::overflow_error 識別子を使い切った場合。
 */
RequestId RequestQueue::SubmitImpl(const RequestTiming i_timing, std::unique_ptr<CommandBase> i_command)
{
    if (m_bShutdown) { throw std::logic_error("終了済みのキューには要求できません。"); }

    // 反映タイミングが不正な場合は Queue() が例外を投げる。
    auto& queue = Queue(i_timing);
    if (m_uNext == std::numeric_limits<RequestId>::max())
    {
        // まずないはずだが、念のためオーバーフローをチェックする。
        throw std::overflow_error("要求の識別子を使い切りました。");
    }

    const auto id = m_uNext;
    const auto inserted = m_instResults.emplace(id, RequestResult{});
    // 抜ける際に勝手に要求結果を巻き戻すためのスコープガード
    struct Rollback
    {
        std::unordered_map<RequestId, RequestResult>& results;
        RequestId id;
        bool committed = false;
        ~Rollback() { if (!committed) { results.erase(id); } }
    };
    auto rollback = Rollback{ m_instResults, id };
    queue.push_back(Pending{ id, std::move(i_command) });
    rollback.committed = true;
    ++m_uNext;
    return inserted.first->first;
}

/**
 * @brief 指定した区切りで処理待ちの要求を受付順に実行する。
 * @param i_timing 反映する区切り。
 * @throws std::invalid_argument 反映タイミングが不正な場合。
 * @throws std::logic_error 再入または終了後に実行した場合。
 */
void RequestQueue::Process(const RequestTiming i_timing)
{
    if (m_bShutdown) { throw std::logic_error("終了済みのキューは処理できません。"); }
    if (m_bProcessing) { throw std::logic_error("要求処理を再入できません。"); }
    auto& queue = Queue(i_timing);

    // プロセスの再入を防ぐためのスコープガード
    struct ProcessingGuard
    {
        bool& running;
        ~ProcessingGuard() { running = false; }
    };

    // キューの処理開始
    m_bProcessing = true;
    const auto guard = ProcessingGuard{ m_bProcessing };
    const auto count = queue.size();
    for (auto index = std::size_t{}; index < count; ++index)
    {
        // キューの先頭を取得
        auto interruption = std::string("要求の処理中に例外が発生しました。");
        auto pending = std::move(queue.front());
        queue.pop_front();
        auto& result = m_instResults.at(pending.id);

        // 要求の処理中に例外が発生した場合にメッセージをリザルトに帰す
        struct FailureGuard
        {
            RequestResult& result;
            std::string message;
            bool committed = false;
            ~FailureGuard()
            {
                if (!committed)
                {
                    result.status = RequestStatus::Failed;
                    result.message = std::move(message);
                }
            }
        };

        // キューのコマンドを実行
        auto failure = FailureGuard{ result, std::move(interruption) };
        auto outcome = pending.command->Execute();
        result.status = outcome.m_instStatus;
        result.entity = std::move(outcome.m_instEntity);
        result.message = std::move(outcome.m_instMessage);

        // コマンドの実行が成功した場合は失敗ガードを解除
        failure.committed = true;
    }
}

/**
 * @brief 状態を取得する。結果は消費しない。
 * @param i_request 要求識別子。
 * @return 現在の結果。
 * @throws std::invalid_argument 存在しない識別子の場合。
 */
RequestResult RequestQueue::GetResult(const RequestId i_request) const
{
    const auto found = m_instResults.find(i_request);
    if (found == m_instResults.end())
    { 
        throw std::invalid_argument("要求の識別子が無効です。");
    }
    return found->second;
}

/**
 * @brief 完了した要求の結果を取得し、保持状態から取り除く。
 * @param i_request 要求識別子。
 * @return 完了した結果。
 * @throws std::invalid_argument 存在しない識別子の場合。
 * @throws std::logic_error 処理待ちの場合。
 */
RequestResult RequestQueue::TakeResult(const RequestId i_request)
{
    const auto found = m_instResults.find(i_request);
    if (found == m_instResults.end()) { throw std::invalid_argument("要求の識別子が無効です。"); }
    if (found->second.status == RequestStatus::Pending)
    {
        throw std::logic_error("処理待ちの要求結果は取得できません。");
    }
    auto result = std::move(found->second);
    m_instResults.erase(found);
    return result;
}

/**
 * @brief 処理待ちの要求を中止し、理由を保持する。
 * @throws std::logic_error 要求処理中に呼び出した場合。
 * @throws std::length_error 中止する要求数がコンテナーの上限を超える場合。
 */
void RequestQueue::Shutdown()
{
    if (m_bProcessing) { throw std::logic_error("要求処理中は終了できません。"); }
    if (m_bShutdown) { return; }

    // 溜まっていた要求分中止するためのメッセージを準備
    auto messages = std::vector<std::string>();
    if (m_instAfter.size() > messages.max_size() ||
        m_instBefore.size() > messages.max_size() - m_instAfter.size())
    {
        throw std::length_error("中止する要求の数が上限を超えました。");
    }
    const auto count = m_instBefore.size() + m_instAfter.size();
    messages.reserve(count);
    for (auto index = std::size_t{}; index < count; ++index)
    {
        messages.emplace_back("キューの終了により要求が中止されました。");
    }

    // 各保留要求に中止メッセージを割り当て、状態をCancelledに更新
    auto index = std::size_t{};
    for (const auto& pending : m_instBefore)
    {
        auto& result    =   m_instResults.at(pending.id);
        result.message  =   std::move(messages[index++]);
        result.status   =   RequestStatus::Cancelled;
    }
    for (const auto& pending : m_instAfter)
    {
        auto& result    =   m_instResults.at(pending.id);
        result.message  =   std::move(messages[index++]);
        result.status   =   RequestStatus::Cancelled;
    }

    // キューの中の要求を全て消し、シャットダウン状態に
    m_instBefore.clear();
    m_instAfter.clear();
    m_bShutdown = true;
}
}
