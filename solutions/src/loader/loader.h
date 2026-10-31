/**
 * @file loader.h
 * @brief 読み込み処理用のヘッダー。
 * @author Ryoma Seita
 * @date 2026-10-27
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_LOADER_LOADER_H
#define DYNAMIC_WIND_DEMO_LOADER_LOADER_H

#include "loader_base.h"
#include "def/loader_common_def.h"


namespace loader
{
    /**
    * @brief   指定されたローダークラスに対応する Loader を定義する。
    * @tparam  LoaderClass LoaderType と対応拡張子一覧を持つローダークラス。
    * @details Loader をコンセプトパターンに従って分岐させるためのクラス。
    */
    template <LoaderType LoaderKind>
    class Loader;
}   

#endif // DYNAMIC_WIND_DEMO_LOADER_LOADER_H