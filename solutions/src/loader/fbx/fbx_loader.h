/**
 * @file fbx_loader.h
 * @brief FBX 読み込み用の Loader 特殊化を定義する。
 * @author Ryoma Seita
 * @date 2026-10-27
 * @par 最終更新日 2026-10-28
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_LOADER_FBX_FBX_LOADER_H
#define DYNAMIC_WIND_DEMO_LOADER_FBX_FBX_LOADER_H

#include "../loader.h"
#include "../../general/model/ModelData.h"

#include <string>

namespace loader
{

/**
 * @brief FBX ファイルを読み込む Loader の特殊化。
 */
template <>
class Loader<LoaderType::FBX> : public LoaderBase
{
public:
    /**
     * @brief 読み込んだ静的モデルデータを参照する。
     * @return 直近の読み込みに成功したモデルデータ。
     */
    const ModelData& GetModelData() const noexcept override;

protected:
    /**
     * @brief FBX ファイルの読み込み処理を実行する。
     * @param i_path 読み込む FBX ファイルの UTF-8 パス。
     * @return 読み込みに成功した場合は true。
     */
    bool LoadImpl(const std::string& i_path) override;

private:
    ModelData m_instModelData;
};

}

namespace loader::fbx
{
// 直感的に使えるように別名を定義する。
using FBXLoader = loader::Loader<loader::LoaderType::FBX>;
}

#endif // DYNAMIC_WIND_DEMO_LOADER_FBX_FBX_LOADER_H