/**
 * @file string_util.h
 * @brief 文字列に関する汎用変換処理を宣言する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_GENERAL_UTIL_STRING_UTIL_H
#define DYNAMIC_WIND_DEMO_GENERAL_UTIL_STRING_UTIL_H

#include <cstddef>
#include <string>

namespace general::util
{

/**
 * @brief 指定範囲の文字データを標準文字列へ変換する。
 * @param i_data 文字データ。nullptr の場合は空文字列を返す。
 * @param i_length 文字データのバイト数。
 * @return 指定されたバイト列を保持する文字列。
 */
std::string ToString(const char* i_data, std::size_t i_length);

}

#endif // DYNAMIC_WIND_DEMO_GENERAL_UTIL_STRING_UTIL_H
