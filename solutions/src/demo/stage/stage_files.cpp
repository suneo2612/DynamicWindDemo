/**
 * @file stage_files.cpp
 * @brief ステージフォルダー内の FBX ファイルを再帰的に検索する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#include "stage_files.h"
#include <algorithm>
#include <cwctype>

namespace demo::stage
{
namespace
{
// 現時点で検索対象とする形式は FBX のみ。
constexpr auto kStageExtension = L".fbx";
}

/**
 * @brief   指定フォルダー以下の FBX ファイルを再帰的に検索する。
 * @param   i_directory 検索対象のフォルダー。
 * @return  パス順に並べた FBX ファイルのパス。
 * @throws  std::filesystem::filesystem_error フォルダーの走査に失敗した場合。
 * @details
 * 現時点では FBX 形式のみを対象とする。拡張子の大文字・小文字を区別せず、
 * ディレクトリのシンボリックリンクはたどらない。
 */
std::vector<std::filesystem::path> FindStages(const std::filesystem::path& i_directory)
{
    auto result = std::vector<std::filesystem::path>{};

    // サブディレクトリを再帰的に走査する
    for (const auto& entry : std::filesystem::recursive_directory_iterator(i_directory))
    {
        // ディレクトリのシンボリックリンクは無視する
        if (!entry.is_regular_file()) { continue; }

        // 拡張子を小文字に変換して比較する
        auto extension = entry.path().extension().wstring();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
             [](wchar_t i_character) {
                return static_cast<wchar_t>(std::towlower(i_character)); 
            }
        );
        if (extension == kStageExtension) { result.push_back(entry.path()); }
    }

    // パス順にソートする
    std::sort(result.begin(), result.end());
    return result;
}

}   // namespace demo::stage
