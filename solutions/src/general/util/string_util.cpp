/**
 * @file string_util.cpp
 * @brief 文字列に関する汎用変換処理を実装する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "string_util.h"

namespace general::util
{

/**
 * @brief 指定範囲の文字データを標準文字列へ変換する。
 * @param i_data 文字データ。nullptr の場合は空文字列を返す。
 * @param i_length 文字データのバイト数。
 * @return 指定されたバイト列を保持する文字列。
 */
std::string ToString(const char* i_data, const std::size_t i_length)
{
    if (i_data == nullptr || i_length == 0)
    {
        return {};
    }
    return { i_data, i_length };
}

}
