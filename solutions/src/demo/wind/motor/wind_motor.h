/**
 * @file wind_motor.h
 * @brief 風発生器の設定、生成要求、および寿命を定義する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_WIND_MOTOR_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_MOTOR_WIND_MOTOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "defs/wind_motor_enums.h"

namespace demo::wind::motor
{

/**
 * @brief 発生器の形状と風の設定。
 * @details 位置と方向はワールド座標で指定する。
 */
struct Settings
{
    Shape                   shape = Shape::Sphere;          // 風が届く範囲の形状
    Mode                    mode = Mode::Continuous;        // 風の発生方法
    std::array<float, 3>    position{0.0F, 0.0F, 0.0F};     // 発生位置（m）
    std::array<float, 3>    direction{0.0F, 1.0F, 0.0F};    // 発生方向。球の中心では既定方向
    float                   speed           =   0.0F;       // 風速（m/s）
    float                   diameter        =   1.0F;       // 球の直径、または円錐・円柱の底面の直径（m）
    float                   length          =   1.0F;       // 円錐・円柱の長さ（m）
    bool                    distanceFalloff =   false;      // 距離に応じて風を弱めるか
    bool                    enabled         =   true;       // 風を発生させるか

    /** @brief 発生器のすべての設定を比較する。 */
    bool operator==(const Settings& i_other) const = default;
};

/**
 * @brief 発生器または生成要求を識別する番号。
 */
using MotorId   =   std::uint64_t;
using RequestId =   std::uint64_t;

/**
 * @brief 発生器の識別子と適用中の設定。
 */
struct Motor
{
    MotorId     id;         // 発生器を識別する番号
    Settings    settings;   // 発生器に適用中の設定

    /** @brief 識別子と設定を比較する。 */
    bool operator==(const Motor& i_other) const = default;
};

/**
 * @brief 生成要求の状態と処理結果。
 * @details 補正が行われた場合は、成功時にその理由と補正後の設定を返す。
 */
struct CreateResult
{
    RequestStatus           status = RequestStatus::Pending;    // 要求の状態
    std::optional<MotorId>  id;                                 // 発生器の識別子（生成成功時）
    std::optional<Settings> settings;                           // 補正後の設定（要求処理後）
    std::string             message;                            // 補正理由または失敗理由
};

/**
 * @brief 発生器の設定変更の結果。
 * @details 設定値を補正した場合は succeeded が false になり、補正後の値と理由を返す。
 */
struct SetResult
{
    bool                    succeeded;   // 補正なしで設定を適用できたか
    Settings                settings;    // 適用された設定
    std::string             message;     // 補正理由
    bool                    removed;     // 設定変更により発生器を削除したか
};

/**
 * @brief 発生器と生成要求を管理する。
 * @details 同一スレッドで使用する。Entity の移動には自動追従しない。
 * 瞬間発生器は Snapshot() の内容を風の更新に反映した後、CompleteInstant() で解放する。
 */
class WindMotors final
{
public:
    /**
     * @brief 通常用途の発生器枠を用意する。
     * @param i_capacity 通常用途の最大数。
     */
    explicit WindMotors(std::size_t i_capacity = 32);

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    /**
     * @brief 通常用途とデバッグ専用の発生器枠を用意する。
     * @param i_capacity 通常用途の最大数。
     * @param i_debugCapacity デバッグ専用の最大数。
     */
    WindMotors(std::size_t i_capacity, std::size_t i_debugCapacity);
#endif

    /**
     * @brief 発生器の生成を受付順に予約する。
     * @param i_settings 発生器の設定。
     * @return 後で結果を取得するための要求番号。
     * @throws std::invalid_argument 設定に非有限値、ゼロ方向または不正な列挙値がある場合。
     * @throws std::overflow_error 識別番号を使い切った場合。
     */
    RequestId RequestCreate(const Settings& i_settings);

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    /**
     * @brief デバッグ専用枠に発生器の生成を予約する。
     * @param i_settings 発生器の設定。
     * @return 後で結果を取得するための要求番号。
     * @throws std::invalid_argument 設定に扱えない値が含まれる場合。
     * @throws std::overflow_error 識別番号を使い切った場合。
     */
    RequestId RequestCreateDebug(const Settings& i_settings);
#endif

    /**
     * @brief 保留中の生成要求を受付順に処理する。
     * @details 容量不足などで生成できない要求も失敗として完了させ、空き待ちにはしない。
     * @throws std::overflow_error 発生器の識別番号を使い切った場合。
     */
    void ProcessRequests();

    /**
     * @brief 要求結果を取得する。完了結果は取得後に破棄する。
     * @param i_request 要求番号。
     * @return 要求の状態と結果。
     * @throws std::invalid_argument 要求番号が存在しない場合。
     */
    CreateResult TakeResult(RequestId i_request);

    /**
     * @brief 発生器の設定を変更する。次に取得する更新用一覧から反映される。
     * @param i_id 対象の発生器。
     * @param i_settings 新しい設定。発生の種類は変更できない。
     * @return 補正なしで適用できたか、適用値、補正理由、および削除の有無。
     * @throws std::invalid_argument 対象がない、設定値が不正、または発生方法を変更した場合。
     */
    SetResult Set(MotorId i_id, const Settings& i_settings);

    /**
     * @brief 発生器を明示的に削除し、枠を解放する。
     * @param i_id 対象の発生器。
     * @throws std::invalid_argument 対象が存在しない場合。
     */
    void Remove(MotorId i_id);

    /**
     * @brief 有効な発生器を、風の更新に使える値の一覧として取得する。
     * @return 設定をコピーした一覧。後の設定変更の影響を受けない。
     */
    std::vector<Motor> Snapshot() const;

    /**
     * @brief 風の更新に反映した瞬間発生器を削除する。
     * @param i_id 反映済みの瞬間発生器。
     * @throws std::invalid_argument 対象が存在しない、または継続発生器の場合。
     */
    void CompleteInstant(MotorId i_id);

private:

    /** 
     *  @brief 発生器と、割り当てられた枠の種類を保持する。 
     */
    struct Entry
    {
        Motor   motor;  // 管理対象の発生器
        bool    debug;  // デバッグ専用枠に属する場合は true
    };

    /** 
     *  @brief 受付順に処理する、検証済みの生成要求。 
     */
    struct Pending
    {
        RequestId   id;         // 要求を識別する番号
        Settings    settings;   // 生成時に指定された設定
        bool        debug;      // デバッグ専用枠を使う場合は true
    };

    /**
     * @brief 指定された枠に生成要求を登録する。
     * @param i_settings 発生器の設定。
     * @param i_debug デバッグ専用枠を使う場合は true。
     * @return 登録した要求の番号。
     * @throws std::invalid_argument 設定値が不正な場合。
     * @throws std::overflow_error 要求番号を使い切った場合。
     */
    RequestId Submit(const Settings& i_settings, bool i_debug);

    /**
     * @brief 識別番号から発生器を検索する。
     * @param i_id 対象の識別番号。
     * @return 発生器を保持する要素への反復子。
     * @throws std::invalid_argument 対象が存在しない場合。
     */
    std::vector<Entry>::iterator Find(MotorId i_id);

    std::size_t                                 m_uCapacity;                    // 通常枠の最大数
    std::size_t                                 m_uDebugCapacity = 0;           // デバッグ専用枠の最大数
    MotorId                                     m_uNextMotor = 1;               // 次に割り当てる発生器番号
    RequestId                                   m_uNextRequest = 1;             // 次に割り当てる要求番号
    std::vector<Entry>                          m_instMotors;                   // 登録済み発生器
    std::deque<Pending>                         m_instPending;                  // 処理待ち要求（受付順）
    std::unordered_map<RequestId, CreateResult> m_instResults;                  // 要求番号ごとの結果
};
}
#endif
