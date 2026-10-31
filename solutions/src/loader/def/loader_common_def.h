/**
 * @file loader_common_def.h
 * @brief Loader の種別など、読み込み処理で共有する定義をまとめる。
 * @author Ryoma Seita
 * @date 2026-10-27
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_LOADER_DEF_LOADER_COMMON_DEF_H
#define DYNAMIC_WIND_DEMO_LOADER_DEF_LOADER_COMMON_DEF_H

#include <map>
#include <string>
#include <vector>

namespace loader
{

// =================================================================
// 列挙値定義
// =================================================================
/**
 * @brief Loader の種別を表す列挙型。
 */
enum class LoaderType
{
    NONE,   // 未定義の Loader
    FBX,    // FBX Loader
    MAX     // 最大値（ループ用）
};


// =================================================================
// 定数定義
// =================================================================
inline const std::map<LoaderType, std::vector<std::string>> LoaderExtensions
{
    { LoaderType::FBX, { ".fbx" } }
};

}

#endif