/**
 * @file component_lifecycle.h
 * @brief Component 初期化と終了の契約を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_LIFECYCLE_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_LIFECYCLE_H

#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

namespace demo::ecs
{

/**
 *  @brief 初期化段階の成功または理由付きの失敗。 
 */
class InitializationResult final
{
public:
    /** @brief 初期化の成功を返す。 */
    static InitializationResult Success() noexcept {
        return InitializationResult(true, {});
    }

    /**
     * @brief 初期化の失敗とその理由を返す。
     * @param i_message 失敗理由。空文字列は指定できない。
     * @return 理由を含む失敗結果。
     * @throws std::invalid_argument 失敗理由が空の場合。
     */
    static InitializationResult Failure(std::string i_message)
    {
        if (i_message.empty())
        {
            throw std::invalid_argument("初期化の失敗理由を指定してください。");
        }
        return InitializationResult(false, std::move(i_message));
    }

    /** @brief 初期化に成功した場合は true を返す。 */
    bool Succeeded() const noexcept { return m_bSucceeded; }
    /** @brief 失敗理由を返す。成功時は空文字列。 */
    const std::string& Message() const noexcept { return m_instMessage; }

private:

    // コンストラクタはこのクラス内でのみ限定
    InitializationResult(const bool i_succeeded, std::string i_message) noexcept
        : m_bSucceeded(i_succeeded), m_instMessage(std::move(i_message)) {}
    bool            m_bSucceeded;   // 初期化に成功したかどうか
    std::string     m_instMessage;  // 初期化に失敗した場合の理由
};

/**
 * @brief   明示的に処理が不要と指定した初期化・終了処理。
 * @tparam  T 処理対象の Component。
 * @details 必要な段階を持たない Component の初期化処理として明示的に使用する。
 */
template<typename T>
struct NoOpComponentInitializer
{
    /**
     * @brief 前初期化で何もしない。
     * @param io_component 対象。
     * @param i_context 参照用の初期化コンテキスト。
     * @return 成功結果。
     */
    template<typename Context>
    static InitializationResult PreInitialize(T& io_component, const Context& i_context) noexcept
    {
        static_cast<void>(io_component);
        static_cast<void>(i_context);
        return InitializationResult::Success();
    }

    /**
     * @brief 初期化で何もしない。
     * @param io_component 対象。
     * @param i_context 参照用の初期化コンテキスト。
     * @return 成功結果。
     */
    template<typename Context>
    static InitializationResult Initialize(T& io_component, const Context& i_context) noexcept
    {
        static_cast<void>(io_component);
        static_cast<void>(i_context);
        return InitializationResult::Success();
    }

    /**
     * @brief 後初期化で何もしない。
     * @param io_component 対象。
     * @param i_context 参照用の初期化コンテキスト。
     * @return 成功結果。
     */
    template<typename Context>
    static InitializationResult PostInitialize(T& io_component, const Context& i_context) noexcept
    {
        static_cast<void>(io_component);
        static_cast<void>(i_context);
        return InitializationResult::Success();
    }

    /**
     * @brief 終了時に何もしない。
     * @param io_component 対象。
     * @param i_context 参照用の初期化コンテキスト。
     */
    template<typename Context>
    static void Terminate(T& io_component, const Context& i_context) noexcept
    {
        static_cast<void>(io_component);
        static_cast<void>(i_context);
    }
};

/**
 * @brief   3段階の初期化と、例外を送出しない終了処理を持っていることを検証する。
 * @tparam  判定対象の型
 * @tparam  T Component の型。
 * @tparam  Context 他の Component を参照するコンテキストの型。
 */
template<typename Initializer, typename T, typename Context>
concept ComponentInitializer = requires(T& io_component, const Context& i_context)
{
    { Initializer::PreInitialize(io_component, i_context) } -> std::same_as<InitializationResult>;      // 前初期化の形式を検証
    { Initializer::Initialize(io_component, i_context) } -> std::same_as<InitializationResult>;         // 初期化の形式を検証
    { Initializer::PostInitialize(io_component, i_context) } -> std::same_as<InitializationResult>;     // 後初期化の形式を検証
    { Initializer::Terminate(io_component, i_context) } noexcept -> std::same_as<void>;                 // 終了処理の形式を検証（例外を送出しないこともチェック）
};
}
#endif
