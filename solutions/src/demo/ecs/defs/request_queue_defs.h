/**
 * @file request_queue_defs.h
 * @brief 要求キューで共有する列挙型、識別子、および結果を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_DEFS_REQUEST_QUEUE_DEFS_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_DEFS_REQUEST_QUEUE_DEFS_H

#include "../entity_registry.h"
#include <cstdint>
#include <optional>
#include <string>

namespace demo::ecs
{

/** 
 *  @brief 要求を反映する更新の区切り。 
 */
enum class RequestTiming
{
    BeforeUpdate,   // 更新前の区切り。
    AfterUpdate     // 更新後の区切り。
};

/**
 *  @brief 呼び出し側が確認する要求の状態。 
 */
enum class RequestStatus
{
    Pending,    // 処理待ち。
    Succeeded,  // 成功。
    Failed,     // 失敗。
    Cancelled   // 中止。
};

/**
 *  @brief 結果を取得するまで有効な要求識別子。
 */
using RequestId = std::uint64_t;

/**
 *  @brief 実行済み要求または処理待ち要求の結果。
 */
struct RequestResult
{
    RequestStatus           status = RequestStatus::Pending;    // 要求の状態(処理待ち、成功、失敗、中止)
    std::optional<Entity>   entity;                             // 関連するエンティティ（存在しない場合もある）
    std::string             message;                            // エラーメッセージや補足情報
};

}   // namespace demo::ecs
#endif
