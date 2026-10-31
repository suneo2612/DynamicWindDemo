/**
 * @file path_util.h
 * @brief UTF-8 文字列とファイルシステムパスの変換を宣言する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_UTIL_PATH_UTIL_H
#define DYNAMIC_WIND_DEMO_GENERAL_UTIL_PATH_UTIL_H

#include <filesystem>
#include <string>
#include <string_view>

namespace general::util
{

/**
 * @brief UTF-8 文字列をファイルシステムのパスへ変換する。
 * @param i_path UTF-8 で表現したパス。
 * @return ファイルシステム操作に使うパス。
 */
std::filesystem::path PathFromUtf8(std::string_view i_path);

/**
 * @brief ファイルシステムのパスを UTF-8 文字列へ変換する。
 * @param i_path 変換するパス。
 * @return UTF-8 で表現したパス。
 */
std::string PathToUtf8(const std::filesystem::path& i_path);

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_UTIL_PATH_UTIL_H
