/**
 * @file loader.cpp
 * @brief 読み込み処理の実装用ファイル。
 * @author Ryoma Seita
 * @date 2026-10-27
 * @par 最終更新日 2026-10-27
 */

#include "loader_base.h"

namespace loader
{

/**
 * @brief 派生ローダーの読み込み結果と失敗理由を管理する。
 * @param i_path 読み込むファイルの UTF-8 パス。
 * @return 派生ローダーの読み込みに成功した場合は true。
 */
bool LoaderBase::Load(const std::string& i_path)
{
    m_instLastError.clear();
    if (LoadImpl(i_path))
    {
        return true;
    }

    if (m_instLastError.empty())
    {
        m_instLastError = "読み込みに失敗しました。";
    }
    return false;
}

/**
 * @brief 直近の読み込み失敗理由を取得する。
 * @return 失敗理由。直近の読み込みが成功した場合は空文字列。
 */
const std::string& LoaderBase::GetLastError() const noexcept
{
    return m_instLastError;
}

/**
 * @brief 呼び出し側に返す読み込み失敗理由を設定する。
 * @param i_error 読み込みに失敗した理由。
 */
void LoaderBase::SetLastError(const std::string& i_error)
{
    m_instLastError = i_error;
}


}