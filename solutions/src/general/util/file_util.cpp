/**
 * @file file_util.cpp
 * @brief ファイルに関する汎用処理を実装する。
 * @author Ryoma Seita
 * @date 2026-10-28
 * @par 最終更新日 2026-10-28
 */

#include "file_util.h"

#include "path_util.h"

#include <fstream>
#include <limits>

namespace general::util
{

bool ReadFile(
    const std::filesystem::path& i_path,
    std::vector<std::uint8_t>& o_bytes,
    std::string& o_error)
{
    auto file = std::ifstream(i_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        o_error = "ファイルを開けません: " + PathToUtf8(i_path);
        return false;
    }

    const auto endPosition = file.tellg();
    if (endPosition <= std::streampos(0))
    {
        o_error = "ファイルが空か、サイズを取得できません: " +
            PathToUtf8(i_path);
        return false;
    }

    const auto fileSize = static_cast<std::streamoff>(endPosition);
    if (fileSize <= 0)
    {
        o_error = "ファイルサイズを取得できません: " + PathToUtf8(i_path);
        return false;
    }

    const auto unsignedFileSize = static_cast<std::uintmax_t>(fileSize);
    if (unsignedFileSize >
            static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
        unsignedFileSize > static_cast<std::uintmax_t>(o_bytes.max_size()))
    {
        o_error = "ファイルサイズが読み込み可能な範囲を超えています: " +
            PathToUtf8(i_path);
        return false;
    }

    o_bytes.resize(static_cast<std::size_t>(unsignedFileSize));
    file.seekg(0, std::ios::beg);
    file.read(
        reinterpret_cast<char*>(o_bytes.data()),
        static_cast<std::streamsize>(unsignedFileSize));
    if (!file || file.gcount() != static_cast<std::streamsize>(unsignedFileSize))
    {
        o_error = "ファイルを最後まで読み込めません: " + PathToUtf8(i_path);
        o_bytes.clear();
        return false;
    }
    return true;
}

}
