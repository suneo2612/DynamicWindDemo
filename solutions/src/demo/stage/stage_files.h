/**
 * @file stage_files.h
 * @brief ステージフォルダー内の FBX ファイルを検索する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_STAGE_STAGE_FILES_H
#define DYNAMIC_WIND_DEMO_DEMO_STAGE_STAGE_FILES_H

#include <filesystem>
#include <vector>

namespace demo::stage
{
/**
 * @brief 指定フォルダー以下の FBX ファイルを再帰的に検索する。
 * @param i_directory 検索対象のフォルダー。
 * @return パス順に並べた FBX ファイルのパス。
 * @throws std::filesystem::filesystem_error フォルダーの走査に失敗した場合。
 * @details
 * 現時点では FBX 形式のみを対象とする。拡張子の大文字・小文字を区別せず、
 * ディレクトリのシンボリックリンクはたどらない。
 */
std::vector<std::filesystem::path> FindStages(const std::filesystem::path& i_directory);
}
#endif
