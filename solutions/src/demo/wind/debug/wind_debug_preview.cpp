/**
 * @file wind_debug_preview.cpp
 * @brief 風の断面プレビューを生成する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "wind_debug_preview.h"
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace demo::wind::debug
{
namespace
{

/**
 * @brief 2D テクスチャ更新で変更する OpenGL 状態を保存し、破棄時に復元する。
 */
struct Texture2DState
{
    GLint active{};  // 保存時に有効だったテクスチャ単位
    GLint binding{}; // テクスチャ単位 0 に結合されていた 2D テクスチャ名

    /**
     * @brief 現在のテクスチャ単位と単位 0 の結合状態を保存する。
     * @pre 有効な OpenGL コンテキスト上で呼び出す。
     */
    Texture2DState()
    {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);       // 現在有効なテクスチャ単位を取得
        glActiveTexture(GL_TEXTURE0);                    // 取得対象をテクスチャ単位 0 に切り替える
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);  // 単位 0 に結合中の 2D テクスチャIDを取得
    }

    /**
     * @brief 単位 0 の結合状態と有効なテクスチャ単位を復元する。
     * @pre 保存時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    ~Texture2DState()
    {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(binding));
        glActiveTexture(static_cast<GLenum>(active));
    }
};

}

/**
 * @brief 風速の XYZ 成分を符号付き RGBA8 プレビュー色に変換する。
 * @param i_wind 風速の XYZ 成分（m/s）。
 * @return RGBA8 色。各成分は -32.7、0、32.7 m/s をそれぞれ 0、128、255 に対応させる。
 * @throws std::invalid_argument 風速に有限でない値が含まれる場合。
 * @details 範囲外の成分は端の色に制限し、アルファ値は 255 にする。
 */
std::array<unsigned char, 4> EncodeWindPreviewColor(const std::array<float, defs::kAxisCount>& i_wind)
{
    constexpr auto maxPreviewSpeed  =   32.7F;      // ビューフォートスケール最大風速をリミットとする
    constexpr auto maxColorValue    =   255.0;      // 色の最大値

    // RGBA の各成分に対応する風速を符号付きで 0～255 に変換する。
    auto rgba = std::array<unsigned char, 4>{};
    for (auto axis = std::size_t{}; axis < i_wind.size(); ++axis)
    {
        if (!std::isfinite(i_wind[axis]))
        {
            throw std::invalid_argument("表示する風速には有限値を指定してください。");
        }
        // 正負の最大風速を色の両端に割り当て、範囲外の値は飽和させる。
        const auto ratio = std::clamp(static_cast<double>(i_wind[axis]) /
                                      static_cast<double>(maxPreviewSpeed), -1.0, 1.0);
        rgba[axis] = static_cast<unsigned char>(std::lround((ratio + 1.0) * 0.5 * maxColorValue));
    }
    // RGBA テクスチャを不透明に保つ。
    rgba[3] = 255;
    return rgba;
}

/**
 * @brief 管理空間の寸法に合わせて断面画像を準備する。
 * @param i_regionSettings 管理空間の設定。
 * @param io_simulation 読み戻し済みの風を持つシミュレーション。
 * @pre 有効な OpenGL コンテキスト上で呼び出す。
 * @throws std::runtime_error 断面テクスチャの作成または初期化に失敗した場合。
 * @details 5 枚の断面位置を管理空間の Y 範囲全体に均等に配置する。
 */
WindDebugPreview::WindDebugPreview(const WindRegionSettings& i_regionSettings,
                                   simulation::WindSimulation& io_simulation)
    : m_stRegionSettings(i_regionSettings), m_instSimulation(io_simulation)
{
    CreatePreviewTextures();
    // 断面を下端から上端まで均等に選び、管理空間全体を確認できるようにする。
    for (auto index = std::size_t{}; index < previewY.size(); ++index)
    {
        previewY[index] = static_cast<int>(
            index * static_cast<std::size_t>(m_stRegionSettings.size[defs::kAxisYIndex] - 1) / (previewY.size() - 1));
    }
}

/**
 * @brief 所有する断面テクスチャを解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
WindDebugPreview::~WindDebugPreview()
{
    glDeleteTextures(static_cast<GLsizei>(previewTextures.size()), previewTextures.data());
}

/**
 * @brief 各断面用の 2D テクスチャを作成し、表示形式を設定する。
 * @pre 有効な OpenGL コンテキスト上で呼び出す。
 * @throws std::runtime_error テクスチャを作成または初期化できない場合。
 * @details 失敗時は作成済みテクスチャを解放する。変更したテクスチャ結合状態はスコープ終了時に復元する。
 */
void WindDebugPreview::CreatePreviewTextures()
{
    const auto state = Texture2DState{};

    // 3DテクスチャのY軸断面描画用テクスチャを作成
    glGenTextures(static_cast<GLsizei>(previewTextures.size()), previewTextures.data());
    if (std::any_of(
        previewTextures.begin(), 
        previewTextures.end(),
        [](const GLuint i_id)
        { 
            return i_id == 0;   // 不正なテクスチャ ID
        })
    )
    {
        glDeleteTextures(static_cast<GLsizei>(previewTextures.size()), previewTextures.data());
        previewTextures.fill(0);
        throw std::runtime_error("風の断面プレビュー用テクスチャを作成できません。");
    }

    // 各断面を XZ サイズの RGBA8 テクスチャとして確保し、セル単位で色を参照する。
    for (const auto texture : previewTextures)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     m_stRegionSettings.size[defs::kAxisXIndex],
                     m_stRegionSettings.size[defs::kAxisZIndex],
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);      // 縮小時に隣接画素を補間せず最近傍画素を使う
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);      // 拡大時に隣接画素を補間せず最近傍画素を使う
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    // 横方向の範囲外参照を端の画素に固定する
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    // 縦方向の範囲外参照を端の画素に固定する
    }
    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(static_cast<GLsizei>(previewTextures.size()), previewTextures.data());
        previewTextures.fill(0);
        throw std::runtime_error("風の断面プレビュー用テクスチャを初期化できません。");
    }
}

/**
 * @brief 各 Y 断面についてセル中心の風速を色に変換し、対応するテクスチャを更新する。
 * @pre 有効な OpenGL コンテキスト上で呼び出す。
 * @throws std::invalid_argument 標本座標または風速が不正な場合。
 * @throws std::runtime_error テクスチャの更新に失敗した場合。
 * @details 標本前の風や管理空間外の標本は WindSimulation の規定の代替風で表示する。
 * 画素は X が最速で変化する順に並べ、変更したテクスチャ結合状態を復元する。
 */
void WindDebugPreview::UpdatePreviews()
{
    const auto state    =   Texture2DState{};                                                       // 更新中に変更するテクスチャの結合状態を復元する
    const auto origin   =   m_instSimulation.Origin();                                              // 標本座標の計算に使う管理空間の原点
    const auto width    =   static_cast<std::size_t>(m_stRegionSettings.size[defs::kAxisXIndex]);   // 断面の X 方向の画素数
    const auto depth    =   static_cast<std::size_t>(m_stRegionSettings.size[defs::kAxisZIndex]);   // 断面の Z 方向の画素数
    auto pixels         =   std::vector<unsigned char>(width * depth * 4);                          // RGBA 各 1 byte の断面画像データ

    // Y 断面ごとにセル中心の風を RGBA 画素へ変換する。
    for (auto index = std::size_t{}; index < previewTextures.size(); ++index)
    {
        const auto y    =   previewY[index];
        for (auto z     =   std::size_t{}; z < depth; ++z)
        {
            for (auto x = std::size_t{}; x < width; ++x)
            {
                // 風を標本座標でサンプリングする
                const auto sample = m_instSimulation.Sample({
                    static_cast<double>(origin[defs::kAxisXIndex]) + static_cast<double>(x) + 0.5,
                    static_cast<double>(origin[defs::kAxisYIndex]) + static_cast<double>(y) + 0.5,
                    static_cast<double>(origin[defs::kAxisZIndex]) + static_cast<double>(z) + 0.5});
                const auto pixel    =   (z * width + x) * 4;
                const auto color    =   EncodeWindPreviewColor(sample.wind);
                std::copy(color.begin(), color.end(), pixels.begin() + static_cast<std::ptrdiff_t>(pixel));
            }
        }
        glBindTexture(GL_TEXTURE_2D, previewTextures[index]);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        m_stRegionSettings.size[defs::kAxisXIndex],
                        m_stRegionSettings.size[defs::kAxisZIndex],
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        pixels.data());
    }
    if (glGetError() != GL_NO_ERROR) { throw std::runtime_error("風の断面プレビューを更新できません。"); }
}

}
