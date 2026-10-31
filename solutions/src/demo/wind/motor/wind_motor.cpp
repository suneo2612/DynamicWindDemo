/**
 * @file wind_motor.cpp
 * @brief 風発生器の設定検証、生成要求、および寿命を管理する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-30
 */
#include "wind_motor.h"
#include "defs/wind_motor_constraints.h"
#include "../defs/wind_region_constants.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace demo::wind::motor
{
    
namespace
{

/**
 * @brief   補正では扱えない設定値を検証する。
 * @param   i_settings 検証する発生器の設定。
 * @throws  std::invalid_argument 非有限値、ゼロ方向、または未定義の列挙値が含まれる場合。
 */
void Validate(const Settings& i_settings)
{
    // 設定中の全ての数値に共通して使う有限値判定を用意する。
    const auto finite = [](const auto i_value) { return std::isfinite(i_value); };
    // 補正処理では安全に扱えない数値を先に拒否する。
    if (!std::all_of(i_settings.position.begin(), i_settings.position.end(), finite) ||
        !std::all_of(i_settings.direction.begin(), i_settings.direction.end(), finite) ||
        !finite(i_settings.speed) || !finite(i_settings.diameter) || !finite(i_settings.length))
    {
        throw std::invalid_argument("発生器の設定には有限値を指定してください。");
    }

    // 形状と発生方法が定義済みの列挙値であることを確認する
    if ((i_settings.shape != Shape::Sphere &&
         i_settings.shape != Shape::Cone &&
         i_settings.shape != Shape::Cylinder) ||
        (i_settings.mode != Mode::Continuous && i_settings.mode != Mode::Instant))
    {
        throw std::invalid_argument("発生器の形状または発生方法が不正です。");
    }

    // 方向を正規化する処理でゼロ除算しないよう、ゼロベクトルを拒否する。
    if (i_settings.direction[demo::wind::defs::kAxisXIndex] == 0.0F &&
        i_settings.direction[demo::wind::defs::kAxisYIndex] == 0.0F &&
        i_settings.direction[demo::wind::defs::kAxisZIndex] == 0.0F)
    {
        throw std::invalid_argument("発生器の方向にはゼロでないベクトルを指定してください。");
    }
}

/**
 * @brief 設定を利用可能な範囲に補正する。
 * @param i_settings 補正する発生器の設定。
 * @return 補正後の設定と、補正した場合の理由。
 */
std::pair<Settings, std::string> Adjust(const Settings& i_settings)
{
    auto settings   =   i_settings;
    auto message    =   std::string{};

    // 複数項目を補正した場合に、理由を一つの結果文字列へまとめる。
    const auto append = [&message](const std::string& i_reason)
    {
        if (!message.empty()) { message += " "; }
        message += i_reason;
    };

    // 方向の長さと風速は別々に扱うため、方向だけを長さ 1 にする。
    const auto length = std::hypot(
        static_cast<double>(settings.direction[demo::wind::defs::kAxisXIndex]),
        static_cast<double>(settings.direction[demo::wind::defs::kAxisYIndex]),
        static_cast<double>(settings.direction[demo::wind::defs::kAxisZIndex]));
    for (auto& component : settings.direction) { component = static_cast<float>(component / length); }

    // 寸法と風速は最小値・最大値を適用し、呼び出し側に補正内容を返す。
    if (settings.diameter < defs::kMinimumSize)
    {
        settings.diameter = defs::kMinimumSize;
        append(std::format("直径を {:g} m に調整しました。", defs::kMinimumSize));
    }
    if (settings.shape != Shape::Sphere && settings.length < defs::kMinimumSize)
    {
        settings.length = defs::kMinimumSize;
        append(std::format("長さを {:g} m に調整しました。", defs::kMinimumSize));
    }
    if (settings.speed < defs::kMinimumSpeed)
    {
        settings.speed = defs::kMinimumSpeed;
        append(std::format("負の風速を {:g} m/s に調整しました。", defs::kMinimumSpeed));
    }
    else if (settings.speed > defs::kMaximumSpeed)
    {
        settings.speed = defs::kMaximumSpeed;
        append(std::format("風速を {:g} m/s に調整しました。", defs::kMaximumSpeed));
    }
    return {settings, message};
}
}

/**
 * @brief 通常用途の発生器枠数を設定して管理器を初期化する。
 * @param i_capacity 通常用途で同時に保持できる発生器の最大数。
 */
WindMotors::WindMotors(const std::size_t i_capacity)
    : m_uCapacity(i_capacity)
#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
    , m_uDebugCapacity(defs::kDefaultDebugCapacity)
#endif
{
}

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
/**
 * @brief 通常用途とデバッグ専用の発生器枠数を設定して管理器を初期化する。
 * @param i_capacity 通常用途で同時に保持できる発生器の最大数。
 * @param i_debugCapacity デバッグ専用枠で同時に保持できる発生器の最大数。
 */
WindMotors::WindMotors(const std::size_t i_capacity, const std::size_t i_debugCapacity)
    : m_uCapacity(i_capacity)
    , m_uDebugCapacity(i_debugCapacity)
{
}
#endif

/**
 * @brief 通常用途の枠を使う生成要求を登録する。
 * @param i_settings 生成する発生器の設定。
 * @return 後で結果を取得するための要求番号。
 * @throws std::invalid_argument 設定に扱えない値が含まれる場合。
 * @throws std::overflow_error 要求番号を使い切った場合。
 */
RequestId WindMotors::RequestCreate(const Settings& i_settings)
{
    return Submit(i_settings, false);
}

#ifdef DYNAMIC_WIND_ENABLE_DEBUG_MOTORS
/**
 * @brief デバッグ専用枠を使う生成要求を登録する。
 * @param i_settings 生成する発生器の設定。
 * @return 後で結果を取得するための要求番号。
 * @throws std::invalid_argument 設定に扱えない値が含まれる場合。
 * @throws std::overflow_error 要求番号を使い切った場合。
 */
RequestId WindMotors::RequestCreateDebug(const Settings& i_settings)
{
    return Submit(i_settings, true);
}
#endif

/**
 * @brief 指定された枠に生成要求を検証して登録する。
 * @param i_settings 生成する発生器の設定。
 * @param i_debug デバッグ専用枠を使用する場合は true。
 * @return 登録した要求の番号。
 * @throws std::invalid_argument 設定に扱えない値が含まれる場合。
 * @throws std::overflow_error 要求番号を使い切った場合。
 */
RequestId WindMotors::Submit(const Settings& i_settings, const bool i_debug)
{
    // 不正な設定をキューへ入れず、要求番号も消費しないよう先に検証する。
    Validate(i_settings);
    if (m_uNextRequest == std::numeric_limits<RequestId>::max())
    {
        throw std::overflow_error("発生器の要求番号を使い切りました。");
    }

    // 要求番号を確保し、結果領域を作成する。
    const auto id = m_uNextRequest;
    m_instResults.emplace(id, CreateResult{});

    // 結果領域の確保後にキュー登録が失敗した場合、未完了結果を残さない。
    try 
    {
        // 風発生器の生成要求をキューへ登録する。
        m_instPending.push_back({id, i_settings, i_debug}); 
    }
    catch (...)
    {
        m_instResults.erase(id);
        throw;
    }
    ++m_uNextRequest;
    return id;
}

/**
 * @brief   保留中の要求を受付順に処理して結果を確定する。
 * @details 枠不足などで生成できない要求も失敗として完了し、空き待ちにしない。
 * @throws  std::overflow_error 発生器の識別番号を使い切った場合。
 */
void WindMotors::ProcessRequests()
{
    while (!m_instPending.empty())
    {
        const auto& pending = m_instPending.front();

        // 設定を補正し、補正後の値とメッセージを取得する。
        const auto  [settings, message] =   Adjust(pending.settings);
        auto        result              =   CreateResult{};
        result.settings                 =   settings;

        // 生成を拒否した要求も完了させ、空き待ちとして残さない。
        // 通常枠とデバッグ枠を別々に数え、要求された枠の上限だけを判定する。
        const auto count = static_cast<std::size_t>(
            std::count_if(
            m_instMotors.begin(),
            m_instMotors.end(),
            [&pending](const auto& i_entry) {
                return i_entry.debug == pending.debug;
            }
        ));

        // デバッグ用途の枠か通常枠かに応じて上限を取得する
        const auto capacity = pending.debug ? m_uDebugCapacity : m_uCapacity;

        // 負の風速は設定変更時と異なり、生成要求では自動補正せず失敗として返す。
        if (pending.settings.speed < 0.0F)
        {
            result.status = RequestStatus::Failed;
            result.message = "負の風速では発生器を生成できません。";
        }
        else if (settings.mode == Mode::Instant && settings.speed == 0.0F)
        {
            result.status = RequestStatus::Failed;
            result.message = std::format("風速 {:g} の瞬間発生器は生成できません。", defs::kMinimumSpeed);
        }
        else if (count >= capacity)
        {
            result.status = RequestStatus::Failed;
            result.message = "発生器の上限に達しています。";
        }
        else
        {
            // 識別番号の重複を避けるため、上限に達した時点で追加を拒否する。
            if (m_uNextMotor == std::numeric_limits<MotorId>::max())
            {
                throw std::overflow_error("発生器の識別番号を使い切りました。");
            }

            // 新しい発生器の識別番号を割り当て、リストに追加する。
            const auto id = m_uNextMotor;
            m_instMotors.push_back({{id, settings}, pending.debug});
            ++m_uNextMotor;

            // 生成に成功した場合の結果を設定する。
            result.status   =   RequestStatus::Succeeded;
            result.id       =   id;
            result.message  =   message;
        }

        // 結果を確定してから要求を外し、処理済み要求が再実行されないようにする。
        m_instResults.at(pending.id)    =   std::move(result);
        m_instPending.pop_front();
    }
}

/**
 * @brief 要求番号に対応する生成結果を取得する。
 * @param i_request 取得する要求番号。
 * @return 要求の状態、識別子、適用設定、および補足メッセージ。
 * @throws std::invalid_argument 要求番号が存在しない場合。
 */
CreateResult WindMotors::TakeResult(const RequestId i_request)
{
    // 要求番号に対応する結果を検索する。
    const auto found = m_instResults.find(i_request);
    if (found == m_instResults.end())
    {
        throw std::invalid_argument("発生器の要求番号が存在しません。");
    }

    // 保留中の結果は後で更新されるため保持し、完了済みの結果だけ取得時に消費する。
    auto result = found->second;
    if (result.status != RequestStatus::Pending) { m_instResults.erase(found); }
    return result;
}

/**
 * @brief   識別番号から登録済み発生器を検索する。
 * @param   i_id 検索する発生器の識別番号。
 * @return  発生器を保持する要素への反復子。
 * @throws  std::invalid_argument 識別番号に一致する発生器がない場合。
 */
std::vector<WindMotors::Entry>::iterator WindMotors::Find(const MotorId i_id)
{
    // 識別番号は外部から指定されるため、見つからない場合は明示的に失敗させる。
    const auto found = std::find_if(
        m_instMotors.begin(),
        m_instMotors.end(),
        [i_id](const auto& i_entry)
        {
            return i_entry.motor.id == i_id; 
        }
    );
    if (found == m_instMotors.end()) { throw std::invalid_argument("発生器が存在しません。"); }
    return found;
}

/**
 * @brief   登録済み発生器の設定を補正して変更する。
 * @param   i_id 設定を変更する発生器の識別番号。
 * @param   i_settings 新しい設定。発生方法は既存値と同じでなければならない。
 * @return  適用値、補正理由、および変更成功・発生器削除の状態。
 * @throws  std::invalid_argument 発生器がない、設定値が不正、または発生方法を変更した場合。
 */
SetResult WindMotors::Set(const MotorId i_id, const Settings& i_settings)
{
    // 設定を変更する前に入力と対象を検証し、状態を部分更新しないようにする。
    Validate(i_settings);

    // 編集する風発生器を検索する
    const auto found = Find(i_id);
    if (found->motor.settings.mode != i_settings.mode)
    {
        throw std::invalid_argument("発生器の発生方法は変更できません。");
    }

    // 設定を補正して適用値と補正理由を取得する
    const auto [settings, message] = Adjust(i_settings);

    // 瞬間発生器をゼロ風速にした場合は、更新対象に残さず枠を解放する。
    const auto removed = settings.mode == Mode::Instant && settings.speed == 0.0F;
    if (removed)
    {
        m_instMotors.erase(found);
    }
    else
    {
        found->motor.settings = settings;
    }

    // 設定変更の結果を返す（補正の有無, 適用値, 補正理由, 削除の有無）
    return {message.empty(), settings, message, removed};
}

/**
 * @brief   登録済み発生器を削除して使用枠を解放する。
 * @param   i_id 削除する発生器の識別番号。
 * @throws  std::invalid_argument 識別番号に一致する発生器がない場合。
 */
void WindMotors::Remove(const MotorId i_id)
{
    m_instMotors.erase(Find(i_id));
}

/**
 * @brief   風の更新に使う有効な発生器のコピーを取得する。
 * @return  enabled が true の発生器を格納した一覧。
 */
std::vector<Motor> WindMotors::Snapshot() const
{
    // 有効な発生器のみをコピーして返す
    auto motors = std::vector<Motor>{};
    motors.reserve(m_instMotors.size());

    // 更新側へ内部状態を公開せず、無効化された発生器も一覧から除外する。
    for (const auto& entry : m_instMotors)
    {
        if (entry.motor.settings.enabled) { motors.push_back(entry.motor); }
    }
    return motors;
}

/**
 * @brief   風の更新に反映済みの瞬間発生器を削除する。
 * @param   i_id 解放する瞬間発生器の識別番号。
 * @throws  std::invalid_argument 発生器がない、または継続発生器を指定した場合。
 */
void WindMotors::CompleteInstant(const MotorId i_id)
{
    const auto found = Find(i_id);
    
    // 継続発生器の誤削除を防ぎ、瞬間発生器だけを明示的な完了で解放する。
    if (found->motor.settings.mode != Mode::Instant)
    {
        throw std::invalid_argument("継続発生器は瞬間風の完了として削除できません。");
    }
    m_instMotors.erase(found);
}

} // namespace demo
