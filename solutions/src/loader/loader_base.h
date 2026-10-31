/**
 * @file loader_base.h
 * @brief 読み込み処理の基底クラスを定義する。
 * @author Ryoma Seita
 * @date 2026-10-27
 * @par 最終更新日 2026-10-27
 */

#pragma once

#ifndef DYNAMIC_WIND_DEMO_LOADER_LOADER_BASE_H
#define DYNAMIC_WIND_DEMO_LOADER_LOADER_BASE_H

#include "../general/model/ModelData.h"

#include <string>

namespace loader
{

/**
 * @brief 読み込み処理の基底クラス。
 */
class LoaderBase
{
public:

    /**
     * @brief 基底ローダーの構築と破棄を行う。
     */
    LoaderBase() = default;

    /**
     * @brief コピーコンストラクタを禁止する。
     */
    LoaderBase(const LoaderBase&) = delete;

    /**
     * @brief コピー代入演算子を禁止する。
     */
    LoaderBase& operator=(const LoaderBase&) = delete;

    /**
     * @brief ムーブコンストラクタを禁止する。
     */
    LoaderBase(LoaderBase&&) = delete;

    /**
     * @brief ムーブ代入演算子を禁止する。
     */
    LoaderBase& operator=(LoaderBase&&) = delete;

    /**
     * @brief 仮想デストラクタ。
     */
    virtual ~LoaderBase() = default;

    /**
     * @brief 指定されたファイルの読み込みを実行する。
     * @param i_path 読み込むファイルの UTF-8 パス。
     * @return 読み込みに成功した場合は true。
     */
    bool Load(const std::string& i_path);

    /**
     * @brief 直近の読み込み失敗理由を取得する。
     * @return 失敗理由。直近の読み込みが成功した場合は空文字列。
     */
    const std::string& GetLastError() const noexcept;

    /**
     * @brief 読み込んだモデルデータを参照する。
     * @return 直近の読み込み結果。失敗後は空のモデルデータ。
     */
    virtual const ModelData& GetModelData() const noexcept = 0;

protected:

    /**
     * @brief 実際の読み込み処理を実装する。
     * @param i_path 読み込むファイルの UTF-8 パス。
     * @return 成功した場合は true、失敗した場合は false を返す。
     */
    virtual bool LoadImpl(const std::string& i_path) = 0;

    /**
     * @brief 呼び出し側に返す読み込み失敗理由を設定する。
     * @param i_error 読み込みに失敗した理由。
     */
    void SetLastError(const std::string& i_error);

private:

    std::string m_instLastError;
};

} // namespace loader

#endif // DYNAMIC_WIND_DEMO_LOADER_LOADER_BASE_H