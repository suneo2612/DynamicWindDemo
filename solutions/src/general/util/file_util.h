/**
 * @file file_util.h
 * @brief ファイルに関する汎用処理を宣言する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_UTIL_FILE_UTIL_H
#define DYNAMIC_WIND_DEMO_GENERAL_UTIL_FILE_UTIL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace general::util
{

/**
 * @brief ファイルをバイナリで読み込む。
 * @param i_path 読み込むファイルのパス。
 * @param o_bytes 読み込んだバイト列。
 * @param o_error 読み込み失敗時の説明。
 * @return 空でないファイルの読み込みに成功した場合は true。
 */
bool ReadFile(
    const std::filesystem::path& i_path,
    std::vector<std::uint8_t>& o_bytes,
    std::string& o_error);

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_UTIL_FILE_UTIL_H
