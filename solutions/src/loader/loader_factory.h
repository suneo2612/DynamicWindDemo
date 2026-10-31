/**
 * @file loader_factory.h
 * @brief Loader の生成を担うクラスを定義する。
 * @author Ryoma Seita
 * @date 2026-10-27
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_LOADER_LOADER_FACTORY_H
#define DYNAMIC_WIND_DEMO_LOADER_LOADER_FACTORY_H

#include <memory>
#include "loader_base.h"
#include "def/loader_common_def.h"

namespace loader
{

/**
 * @brief Loader を生成するための静的クラス。
 */
class LoaderFactory final
{
public:
    
    /**
     * @brief インスタンス化を禁止する。
     */
    LoaderFactory() = delete;


    /**
     * @brief 指定されたパスに対応する Loader を生成する。
     * @param i_path ファイルパス。
     * @return 生成された Loader のポインタ。呼び出し側が所有し、対応する Loader がない場合は nullptr。
     */
    static std::unique_ptr<LoaderBase> CreateLoader(const std::string& i_path);

private:

    /**
     * @brief 指定されたタイプに対応する Loader を生成する。
     * @param i_type Loader の種類。
     * @return 生成された Loader のポインタ。対応する Loader がない場合は nullptr。
     */
    static std::unique_ptr<LoaderBase> CreateLoaderByType(LoaderType i_type);
};

} // namespace loader

#endif // DYNAMIC_WIND_DEMO_LOADER_LOADER_FACTORY_H
