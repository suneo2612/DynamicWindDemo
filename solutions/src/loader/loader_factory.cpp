/**
 * @file loader_factory.cpp
 * @brief LoaderFactory の関数を定義する。
 * @author Ryoma Seita
 * @date 2026-10-27
 */

#include <algorithm>
#include <map>

#include "fbx/fbx_loader.h"
#include "loader_factory.h"

namespace loader
{

namespace
{

using LoaderCreator = std::unique_ptr<LoaderBase> (*)();

/**
 * @brief ファイルパスの末尾から拡張子を取得する。
 * @param i_path UTF-8 のファイルパス。
 * @return 拡張子。拡張子がない場合は空文字列。
 */
std::string GetExtension(const std::string& i_path)
{
    const auto separator = i_path.find_last_of("/\\");
    const auto filenameStart =
        separator == std::string::npos ? 0 : separator + 1;
    const auto dot = i_path.find_last_of('.');
    if (dot == std::string::npos || dot == filenameStart)
    {
        return {};
    }
    return i_path.substr(dot);
}

/**
 * @brief Loader の生成関数を保持するマップ。
 */
const std::map<LoaderType, LoaderCreator> LoaderCreators
{
    {
        LoaderType::FBX,
        []() -> std::unique_ptr<LoaderBase>
        {
            return std::make_unique<Loader<LoaderType::FBX>>();
        }
    }
};

} // namespace

/**
 * @brief 指定されたパスに対応する Loader を生成する。
 * @param i_path ファイルパス。
 * @return 生成された Loader のポインタ。呼び出し側が所有し、対応する Loader がない場合は nullptr。
 */
std::unique_ptr<LoaderBase> LoaderFactory::CreateLoader(const std::string& i_path)
{
    // 空チェック
    if (i_path.empty())
    {
        return nullptr;
    }

    const auto extension = GetExtension(i_path);
    for (const auto& [loaderType, extensions] : LoaderExtensions)
    {
        if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end())
        {
            return CreateLoaderByType(loaderType);
        }
    }

    return nullptr;
}


/**
 * @brief 指定されたタイプに対応する Loader を生成する。
 * @param i_type Loader の種類。
 * @return 生成された Loader のポインタ。呼び出し側が所有し、対応する Loader がない場合は nullptr。
 */
std::unique_ptr<LoaderBase> LoaderFactory::CreateLoaderByType(const LoaderType i_type)
{
    const auto creator = LoaderCreators.find(i_type);
    if (creator == LoaderCreators.end())
    {
        return nullptr;
    }

    return creator->second();
}


} // namespace loader