/**
 * @file path_util.cpp
 * @brief UTF-8 文字列とファイルシステムパスの変換を実装する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "path_util.h"

namespace general::util
{

std::filesystem::path PathFromUtf8(const std::string_view i_path)
{
    std::u8string pathString;
    pathString.reserve(i_path.size());
    for (const unsigned char character : i_path)
    {
        pathString.push_back(static_cast<char8_t>(character));
    }
    return std::filesystem::path(pathString);
}

std::string PathToUtf8(const std::filesystem::path& i_path)
{
    const std::u8string pathString = i_path.u8string();
    std::string result;
    result.reserve(pathString.size());
    for (const char8_t character : pathString)
    {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

}
