/**
 * @file component_catalog.h
 * @brief Component の識別子とコンパイル時の依存関係を定義する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_CATALOG_H
#define DYNAMIC_WIND_DEMO_DEMO_ECS_COMPONENT_CATALOG_H

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

namespace demo::ecs
{

/** 
 * @brief Component の種類を表す識別子。
 * @details
 * Component の種類を追加する場合は、ここに列挙する。
 */
enum class ComponentKind : std::uint32_t
{
    Transform,
    Model
};

/**
 * @brief Component ごとの識別子、必須 Component、初期化優先度を指定する。
 * @tparam T Component の型。
 * @details 各 Component に対して特殊化し、kind、dependencies、priority を定義する。
 */
template<typename T>
struct ComponentTraits;

/**
 * @brief Component の静的な定義がそろっていることを検証する。
 * @tparam T Component の型。
 */
template<typename T>
concept Component = requires
{
    // kind が ComponentKind 型であることを検証
    requires std::same_as<std::remove_cv_t<decltype(ComponentTraits<T>::kind)>, ComponentKind>; // 
    // priority が int 型であることを検証
    requires std::same_as<std::remove_cv_t<decltype(ComponentTraits<T>::priority)>, int>;
    // dependencies が ComponentKind の配列であることを検証
    std::span<const ComponentKind>{ComponentTraits<T>::dependencies};
};

namespace detail
{
/**
 * @brief   Component カタログの妥当性を検証する。（識別子の重複や依存関係の循環をチェックする）
 * @tparam  T 登録する Component 型のパック。
 * @return  妥当であれば true、そうでなければ false。
 */
template<Component... T>
consteval bool ValidComponentCatalog()
{
    // Component の識別子の重複をチェックする。
    constexpr auto count = sizeof...(T);
    constexpr auto kinds = std::array<ComponentKind, count>{ ComponentTraits<T>::kind... };
    auto edges = std::array<std::array<bool, count>, count>{};
    for (auto i = std::size_t{}; i < count; ++i)
    {
        for (auto j = i + 1; j < count; ++j)
        {
            if (kinds[i] == kinds[j]) { return false; }
        }
    }

    // edges[依存元][依存先] を記録し、後段の循環依存検査に使う。
    auto valid = true;
    auto current = std::size_t{};
    // U の依存先識別子を、カタログ内の添字に変換して辺として登録する。
    const auto addDependencies = [&]<typename U>()
    {
        for (const auto dependency : ComponentTraits<U>::dependencies)
        {
            auto found = false;
            // 依存先の識別子に対応する ComponentKind の位置を探す。
            for (auto j = std::size_t{}; j < count; ++j)
            {
                if (kinds[j] == dependency)
                {
                    // 同じ依存先が既に登録されていれば、依存定義が重複している。
                    if (edges[current][j]) { valid = false; }
                    edges[current][j] = true;
                    found = true;
                    break;
                }
            }
            // カタログにない依存先は満たせないため、不正とする。
            if (!found) { valid = false; }
        }
        ++current;
    };
    // 依存先の登録に問題があれば不正とする。
    (addDependencies.template operator()<T>(), ...);
    if (!valid) { return false; }

    // 依存先がすべて解決済みの Component を順に確定し、依存グラフに循環がないか調べる。
    // resolved[i] は kinds[i] の依存関係を解決済みかどうかを表す。
    auto resolved = std::array<bool, count>{};
    // 1回の走査で1つずつ確定するため、Component 数と同じ回数だけ試す。
    for (auto pass = std::size_t{}; pass < count; ++pass)
    {
        // この走査で確定できる Component があったかを記録する。
        auto found = false;
        for (auto i = std::size_t{}; i < count; ++i)
        {
            // 解決済みの Component は再度調べない。
            if (resolved[i]) { continue; }
            auto ready = true;
            for (auto j = std::size_t{}; j < count; ++j)
            {
                // 未解決の依存先が残っている Component は、まだ処理できない。
                if (edges[i][j] && !resolved[j]) { ready = false; break; }
            }
            if (ready)
            {
                // 依存先がすべて解決済みなので、この Component も確定できる。
                resolved[i] = true;
                found = true;
                // 1回の走査で確定するのは1つだけにし、次の走査で依存先として扱う。
                break;
            }
        }
        // 未解決の Component が残るのに1つも確定できない場合、依存関係が循環している。
        if (!found) { return false; }
    }
    return true;
}
}   // namespace detail

/**
 * @brief Component の依存関係を検証し、初期化・終了の順序を決定する。
 * @tparam T 登録する Component 型。宣言順は同じ優先度での登録順を表す。
 * @details 識別子の重複、未登録の依存先、重複した依存先、循環はコンパイル時に拒否する。
 */
template<Component... T>
class ComponentCatalog final
{
    static_assert(detail::ValidComponentCatalog<T...>(),
        "Component の識別子または依存関係が不正です。");

public:
    /**
     * @brief 追加対象を初期化する順序を求める。
     * @param i_add         追加する Component の識別子。
     * @param i_existing    既に存在する Component の識別子。
     * @return 依存先を優先し、残りを優先度の高い順・登録順に並べた識別子。
     * @throws std::invalid_argument 未登録、重複、依存不足がある場合。
     */
    static std::vector<ComponentKind> InitializationOrder(
        std::span<const ComponentKind> i_add,
        std::span<const ComponentKind> i_existing)
    {
        // テンプレートパラメータからコンポーネントの種別と優先度を取得
        constexpr auto count        =   sizeof...(T);
        constexpr auto kinds        =   std::array<ComponentKind, count>{ ComponentTraits<T>::kind... };
        constexpr auto priorities   =   std::array<int, count>{ ComponentTraits<T>::priority... };


        auto added      =   std::array<bool, count>{};  // 追加対象の存在を管理
        auto existing   =   std::array<bool, count>{};  // 既存の存在を管理
        // コンポーネント識別子配列から指定したコンポーネント種別を指すインデックスを取得するラムダ
        const auto find = [&kinds](const ComponentKind i_kind)
        {
            for (auto i = std::size_t{}; i < count; ++i)
            {
                if (kinds[i] == i_kind) { return i; }
            }
            throw std::invalid_argument("未登録の Component が指定されました。");
        };

        // 既存物重複と追加物重複をチェック
        for (const auto kind : i_existing)
        {
            const auto index = find(kind);
            if (existing[index]) { throw std::invalid_argument("Component が重複しています。"); }
            existing[index] = true;
        }

        // 追加物重複チェック
        for (const auto kind : i_add)
        {
            const auto index = find(kind);
            if (existing[index] || added[index])
            {
                throw std::invalid_argument("追加する Component が重複しています。");
            }
            added[index] = true;
        }

        // 行=依存元、列=依存先, 要素=依存性 のグラフをカテゴリに登録されたコンポーネントごとに構築する
        auto edges  =   std::array<std::array<bool, count>, count>{};   // 依存関係グラフ
        auto row    =   std::size_t{};                                  // グラフ行インデックス
        const auto addDependencies = [&]<typename U>()
        {
            // 指定されたコンポーネントUの依存関係を追加
            for (const auto dependency : ComponentTraits<U>::dependencies)
            {
                // 依存先のインデックスを取得
                const auto index = find(dependency);

                // 依存関係をグラフに追加
                edges[row][index] = true;
                if ((added[row] || existing[row]) && !added[index] && !existing[index])
                {
                    throw std::invalid_argument("必須 Component が不足しています。");
                }
                if (existing[row] && added[index])
                {
                    throw std::invalid_argument("既存 Component の必須依存先が不足しています。");
                }
            }
            ++row;
        };
        (addDependencies.template operator()<T>(), ...);

        // 依存関係に基づき追加順序を決定
        auto ordered = std::vector<ComponentKind>();
        ordered.reserve(i_add.size());
        auto resolved = existing;
        while (ordered.size() < i_add.size())
        {
            auto selected = count;
            for (auto i = std::size_t{}; i < count; ++i)
            {
                // i 番目のコンポーネントが追加対象か既に並べ終えたにあるか確認
                if (!added[i] || resolved[i]) { continue; }

                // 依存関係にあるコンポーネントがすべて解決済みか確認
                auto ready = true;
                for (auto j = std::size_t{}; j < count; ++j)
                {
                    if (edges[i][j] && !resolved[j]) 
                    {
                        ready = false;
                        break;
                    }
                }

                // 最も大きな優先度のコンポーネントを選択
                if (ready && (selected == count || priorities[i] > priorities[selected]))
                {
                    selected = i;
                }
            }
            if (selected == count)
            {
                throw std::logic_error("Component の依存順を決定できません。");
            }
            // 選択物の優先度ソートが完了したことを示すために resolved を更新
            resolved[selected] = true;
            ordered.push_back(kinds[selected]);
        }
        return ordered;
    }

    /**
     * @brief 他の Component が残る状態で単独削除できるか検証する。
     * @param i_remove 削除する Component。
     * @param i_existing 現在存在する Component。
     * @throws std::invalid_argument 削除対象がなく、または依存する Component が残る場合。
     */
    static void ValidateRemoval(ComponentKind i_remove, std::span<const ComponentKind> i_existing)
    {
        // 初期化順序を取得して依存関係を確認（未登録、重複、依存不足チェック）
        const auto ordered = InitializationOrder({}, i_existing);
        static_cast<void>(ordered);     // 未使用変数の警告を抑制する

        // 削除対象が存在するか確認
        auto found = false;
        for (const auto kind : i_existing) 
        {
            if (kind == i_remove)
            {
                found = true;
                break;
            }
        }
        if (!found) { throw std::invalid_argument("削除する Component がありません。"); }

        
        auto blocked = false;
        const auto check = [&]<typename U>()
        {
            // 登録されている Component の中で、現在の U の Component が存在するか確認
            auto present = false;
            for (const auto kind : i_existing)
            {
                if (kind == ComponentTraits<U>::kind) 
                {
                    present = true;
                    break;
                } 
            }

            // 現在の U の Component が存在し、かつ削除対象でない場合、依存関係を確認
            if (present && ComponentTraits<U>::kind != i_remove)
            {
                // 依存関係があれば削除対象をブロック
                for (const auto dependency : ComponentTraits<U>::dependencies)
                {
                    if (dependency == i_remove) 
                    { 
                        blocked = true;
                    }
                }
            }
        };
        (check.template operator()<T>(), ...);
        if (blocked) { throw std::invalid_argument("他の Component が削除対象に依存しています。"); }
    }
};  
}   // namespace demo::ecs
#endif
