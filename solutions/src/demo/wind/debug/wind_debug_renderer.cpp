/**
 * @file wind_debug_renderer.cpp
 * @brief 風の線分と矢印を描画する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "wind_debug_renderer.h"
#include "shader_sources.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace demo::wind::debug
{
namespace
{
/**
 * @brief 線分の頂点数が描画とバッファーのサイズの上限に収まるか判定する。
 * @param i_count 頂点数。
 * @return 頂点が 2 点ずつの組になり、両方の上限に収まる場合は true。
 */
constexpr bool IsValidWireframeVertexCount(const std::size_t i_count)
{
    using Vertex = std::array<float, defs::kAxisCount>;
    // バイト数の乗算前に上限を割り算で換算し、オーバーフローを防ぐ。
    return i_count % 2 == 0 &&
           i_count <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) &&
           i_count <= static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(Vertex);
}

// 頂点がない場合と最大の偶数頂点数は許可し、それを超える場合は拒否する。
constexpr auto kMaxWireframeVertices = std::min(
    static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()),
    static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
        sizeof(std::array<float, defs::kAxisCount>));
static_assert(IsValidWireframeVertexCount(0));
static_assert(!IsValidWireframeVertexCount(1));
static_assert(IsValidWireframeVertexCount(kMaxWireframeVertices - kMaxWireframeVertices % 2));
static_assert(!IsValidWireframeVertexCount(kMaxWireframeVertices - kMaxWireframeVertices % 2 + 2));

/**
 * @brief 線分描画に渡す頂点数、頂点座標と色を検証する。
 * @param i_vertices 2 点ずつの線分を表す頂点。
 * @param i_color 線分の RGB 色。
 * @throws std::invalid_argument 頂点数が不正、または頂点や色に非有限値が含まれる場合。
 */
void ValidateWireframeInputs(const std::vector<std::array<float, defs::kAxisCount>>& i_vertices,
                             const std::array<float, 3>& i_color)
{
    const auto finite = [](const float i_value) { return std::isfinite(i_value); };
    if (!IsValidWireframeVertexCount(i_vertices.size()) ||
        !std::all_of(i_color.begin(), i_color.end(), finite) ||
        !std::all_of(i_vertices.begin(), i_vertices.end(), [&](const auto& i_vertex)
        {
            return std::all_of(i_vertex.begin(), i_vertex.end(), finite);
        }))
    {
        throw std::invalid_argument("風の表示用の頂点または色が不正です。");
    }
}

/**
 * @brief この描画処理が変更する OpenGL の結合状態を保存し、破棄時に復元する。
 */
struct DrawState
{
    GLint program{}; // 使用中だったプログラム名
    GLint vao{};     // 結合されていた頂点配列オブジェクト名
    GLint buffer{};  // GL_ARRAY_BUFFER に結合されていたバッファー名

    /**
     * @brief 描画後に戻せるよう現在のプログラムと結合状態を保存する。
     * @pre 有効な OpenGL コンテキスト上で呼び出す。
     */
    DrawState()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);        // 使用中のプログラム名を取得
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);       // 結合中の頂点配列オブジェクト名を取得
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);    // GL_ARRAY_BUFFER に結合中のバッファー名を取得
    }

    /**
     * @brief 保存したプログラムと結合状態を復元する。
     * @pre 保存時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    ~DrawState()
    {
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    }
};

/**
 * @brief 矢印描画で変更するテクスチャ単位と 3D テクスチャ・サンプラーの結合を保存し、復元する。
 */
struct Texture3DState
{
    GLint active{};                                 // 保存時に有効だったテクスチャ単位
    std::array<GLint, defs::kAxisCount> textures{}; // 軸ごとのテクスチャ単位に結合されていた 3D テクスチャ名
    std::array<GLint, defs::kAxisCount> samplers{}; // 軸ごとのテクスチャ単位に結合されていたサンプラー名

    /**
     * @brief コンストラクタ
     * @pre 有効な OpenGL コンテキスト上で呼び出す。
     * @details コンストラクタは、現在のテクスチャ単位と 3D テクスチャ・サンプラーの結合状態を保存する。
     */
    Texture3DState()
    {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        for (auto index = std::size_t{}; index < textures.size(); ++index)
        {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glGetIntegerv(GL_TEXTURE_BINDING_3D, &textures[index]);
            glGetIntegeri_v(GL_SAMPLER_BINDING, static_cast<GLuint>(index), &samplers[index]);
        }
    }
    /**
     * @brief デストラクタ
     * @pre 保存時と同じ有効な OpenGL コンテキスト上で呼び出す。
     * @details デストラクタは、保存したテクスチャ単位と 3D テクスチャ・サンプラーの結合状態を復元する。
     */
    ~Texture3DState()
    {
        for (auto index = std::size_t{}; index < textures.size(); ++index)
        {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(textures[index]));
            glBindSampler(static_cast<GLuint>(index), static_cast<GLuint>(samplers[index]));
        }
        glActiveTexture(static_cast<GLenum>(active));
    }
};

/**
 * @brief コンパイル済みシェーダーを所有し、破棄時に解放する。
 */
struct Shader
{
    GLuint id{}; // コンパイル済みシェーダー

    /**
     * @brief 指定された種類のシェーダーを作成し、ソースをコンパイルする。
     * @param i_type 頂点またはフラグメントなどのシェーダーの種類。
     * @param i_source コンパイルする終端文字付きソース文字列。
     * @pre 有効な OpenGL コンテキスト上で呼び出す。
     * @throws std::runtime_error シェーダーの作成またはコンパイルに失敗した場合。
     */
    Shader(const GLenum i_type, const char* i_source)
    {
        // 指定されたタイプのシェーダーを作成
        id = glCreateShader(i_type);
        if (id == 0) { throw std::runtime_error("風の表示用シェーダーを作成できません。"); }

        // シェーダーのソースを設定してコンパイル
        glShaderSource(id, 1, &i_source, nullptr);
        glCompileShader(id);
        auto success = GLint{};
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (success != GL_TRUE)
        {
            auto length = GLint{};
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
            auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
            glGetShaderInfoLog(id, length, nullptr, log.data());
            glDeleteShader(id);
            id = 0;
            throw std::runtime_error("風の表示用シェーダーのコンパイルに失敗しました: " + log);
        }
    }
    /**
     * @brief 所有するシェーダーを解放する。
     * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    ~Shader() { if (id != 0) { glDeleteShader(id); } }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
};

}

/**
 * @brief 線分描画に必要な OpenGL 資源を所有する。
 */
struct WireframeRenderer::Impl
{
    GLuint program{}; // 線分の描画プログラム
    GLuint vao{};     // 頂点の読み取り設定
    GLuint buffer{};  // 線分の頂点を転送するバッファー名

    /**
     * @brief 作成済みの線分描画資源を解放する。
     * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    ~Impl()
    {
        if (buffer != 0) { glDeleteBuffers(1, &buffer); }
        if (vao != 0) { glDeleteVertexArrays(1, &vao); }
        if (program != 0) { glDeleteProgram(program); }
    }
};

/**
 * @brief 線分描画用のプログラム、頂点配列とバッファーを作成する。
 * @pre 有効な OpenGL コンテキスト上で呼び出す。
 * @throws std::runtime_error シェーダー、プログラムまたは描画資源の作成に失敗した場合。
 */
WireframeRenderer::WireframeRenderer() : m_instImpl(std::make_unique<Impl>())
{
    // 描画資源の設定中に変更する結合状態を保存する。
    const auto state    =   DrawState{};
    const auto vertex   =   Shader(GL_VERTEX_SHADER, shaders::windDebugVertex);
    const auto fragment =   Shader(GL_FRAGMENT_SHADER, shaders::windDebugFragment);

    // 頂点, フラグメントシェーダーをプログラムにセットする。
    auto& impl = *m_instImpl;
    impl.program = glCreateProgram();
    if (impl.program == 0) { throw std::runtime_error("風の表示用プログラムを作成できません。"); }
    glAttachShader(impl.program, vertex.id);
    glAttachShader(impl.program, fragment.id);
    glLinkProgram(impl.program);
    auto success = GLint{};
    glGetProgramiv(impl.program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE)
    {
        auto length = GLint{};
        glGetProgramiv(impl.program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(impl.program, length, nullptr, log.data());
        throw std::runtime_error("風の表示用プログラムをリンクできません: " + log);
    }
    
    // XYZ 座標の配列を頂点属性として読み込めるようにする。
    glGenVertexArrays(1, &impl.vao);
    glGenBuffers(1, &impl.buffer);
    if (impl.vao == 0 || impl.buffer == 0) { throw std::runtime_error("風の表示用資源を作成できません。"); }
    glBindVertexArray(impl.vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl.buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, static_cast<GLint>(defs::kAxisCount), GL_FLOAT, GL_FALSE,
                          sizeof(std::array<float, defs::kAxisCount>), nullptr);
    if (glGetError() != GL_NO_ERROR) { throw std::runtime_error("風の表示用資源を設定できません。"); }
}

/**
 * @brief 所有する線分描画用の GPU 資源を解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
WireframeRenderer::~WireframeRenderer() = default;

/**
 * @brief ワールド座標の頂点を 2 点ずつ線分として描画する。
 * @param i_vertices 線分の両端を順に並べた頂点。
 * @param i_camera 視点と視線を取得するカメラ。
 * @param i_aspect 画面の幅と高さの比率。
 * @param i_color 線分の RGB 色。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 * @throws std::invalid_argument 頂点数、頂点、色または画面比率が不正な場合。
 * @throws std::runtime_error シェーダーの設定取得または GPU 描画に失敗した場合。
 */
void WireframeRenderer::Draw(const std::vector<std::array<float, defs::kAxisCount>>& i_vertices,
                             const render::Camera& i_camera,
                             const float i_aspect, const std::array<float, 3>& i_color) const
{
    ValidateWireframeInputs(i_vertices, i_color);

    // 空の頂点列でも画面比率を検証し、描画に必要な状態だけを一時的に変更する。
    const auto projection = render::Camera::Projection(i_aspect);
    if (i_vertices.empty()) { return; }
    const auto view     =   i_camera.View();
    const auto state    =   DrawState{};
    const auto& impl    =   *m_instImpl;

    // シェーダーの使用を開始する
    glUseProgram(impl.program);
    const auto viewLocation         =   glGetUniformLocation(impl.program, "view");
    const auto projectionLocation   =   glGetUniformLocation(impl.program, "projection");
    const auto colorLocation        =   glGetUniformLocation(impl.program, "lineColor");
    if (viewLocation < 0 || projectionLocation < 0 || colorLocation < 0)
    {
        throw std::runtime_error("風の表示用シェーダーの設定が見つかりません。");
    }

    // ビュー行列、射影行列、線分の色をシェーダーに設定する
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, view.data());                 // ビュー行列を設定
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, projection.data());     // 射影行列を設定
    glUniform3fv(colorLocation, 1, i_color.data());                             // 線分の色を設定
    glBindVertexArray(impl.vao);                                                // 頂点配列オブジェクトをバインド
    glBindBuffer(GL_ARRAY_BUFFER, impl.buffer);                                 // 頂点バッファをバインド
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(i_vertices.size() * sizeof(i_vertices[0])),
                 i_vertices.data(), GL_STREAM_DRAW);     // 頂点データを GPU に転送
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(i_vertices.size()));         // 線分を描画
    if (glGetError() != GL_NO_ERROR)
    {
        throw std::runtime_error("風の形状を描画できません。");
    }
}

/**
 * @brief 風向きの矢印をセルごとに描画するための OpenGL 資源を所有する。
 */
struct WindArrowRenderer::Impl
{
    GLuint program{}; // 矢印の描画プログラム名
    GLuint vao{};     // 描画に使用する頂点配列オブジェクト名
    GLuint sampler{}; // 風速テクスチャを参照するサンプラー名

    /**
     * @brief 作成済みの矢印描画資源を解放する。
     * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    ~Impl()
    {
        if (sampler != 0) { glDeleteSamplers(1, &sampler); }
        if (vao != 0) { glDeleteVertexArrays(1, &vao); }
        if (program != 0) { glDeleteProgram(program); }
    }
};

/**
 * @brief 風向きの矢印を描画するプログラムと GPU 資源を作成する。
 * @pre 有効な OpenGL 4.3 コンテキスト上で呼び出す。
 * @throws std::runtime_error シェーダー、プログラムまたは描画資源の作成に失敗した場合。
 */
WindArrowRenderer::WindArrowRenderer() : m_instImpl(std::make_unique<Impl>())
{
    // 頂点シェーダーとフラグメントシェーダーをコンパイルしてプログラムにリンクする
    const auto vertex = Shader(GL_VERTEX_SHADER, shaders::windArrowVertex);
    const auto fragment = Shader(GL_FRAGMENT_SHADER, shaders::windArrowFragment);
    auto& impl = *m_instImpl;
    impl.program = glCreateProgram();
    if (impl.program == 0) { throw std::runtime_error("風向きの表示用プログラムを作成できません。"); }
    glAttachShader(impl.program, vertex.id);
    glAttachShader(impl.program, fragment.id);
    glLinkProgram(impl.program);
    auto success = GLint{};
    glGetProgramiv(impl.program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE)
    {
        auto length = GLint{};
        glGetProgramiv(impl.program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(impl.program, length, nullptr, log.data());
        throw std::runtime_error("風向きの表示用プログラムをリンクできません: " + log);
    }

    // 頂点配列オブジェクトとサンプラーを作成する
    glGenVertexArrays(1, &impl.vao);
    glGenSamplers(1, &impl.sampler);
    if (impl.vao == 0 || impl.sampler == 0)
    {
        throw std::runtime_error("風向きの表示用資源を作成できません。");
    }

    // セルごとの風速を周囲の値と補間せず参照する。
    glSamplerParameteri(impl.sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(impl.sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (glGetError() != GL_NO_ERROR) 
    {
        throw std::runtime_error("風向きの表示用資源を設定できません。"); 
    }
}

/**
 * @brief 所有する矢印描画用の GPU 資源を解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
WindArrowRenderer::~WindArrowRenderer() = default;

/**
 * @brief GPU の風速テクスチャから各セルの風向きを矢印として描画する。
 * @param i_textures XYZ 成分順の符号付き 16.16 固定小数点 R32I 3D テクスチャ名。
 * @param i_origin 管理空間の最小 XYZ 座標。
 * @param i_size XYZ 各軸のセル数。テクスチャの寸法と一致すること。
 * @param i_camera 視点と視線を取得するカメラ。
 * @param i_aspect 画面の幅と高さの比率。
 * @pre 作成時と同じ有効な OpenGL 4.3 コンテキスト上で呼び出す。
 * @throws std::invalid_argument テクスチャ名、セル数または画面比率が不正な場合。
 * @throws std::runtime_error シェーダーの設定取得または GPU 描画に失敗した場合。
 */
void WindArrowRenderer::Draw(const std::array<unsigned int, defs::kAxisCount>& i_textures,
                             const std::array<int, defs::kAxisCount>& i_origin,
                             const std::array<int, defs::kAxisCount>& i_size,
                             const render::Camera& i_camera, const float i_aspect) const
{
    // 描画回数を表す整数型の上限を超えないよう、乗算前にセル数を検証する。
    auto count = std::uint64_t{1};
    for (auto axis = std::size_t{}; axis < i_size.size(); ++axis)
    {
        if (i_size[axis] <= 0 || 
            i_textures[axis] == 0 ||
            count > static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max()) / static_cast<std::uint64_t>(i_size[axis]))
        {
            throw std::invalid_argument("風向きの表示用テクスチャまたはセル数が不正です。");
        }
        count *= static_cast<std::uint64_t>(i_size[axis]);
    }

    // ビュー行列、射影行列、線分の色をシェーダーに設定する前に、OpenGL のエラーを確認する。
    const auto projection   =   render::Camera::Projection(i_aspect);
    const auto view         =   i_camera.View();
    const auto state        =   DrawState{};
    const auto textureState =   Texture3DState{};
    const auto& impl        =   *m_instImpl;
    glUseProgram(impl.program);
    const auto uniform = [&](const char* i_name)
    {
        const auto location = glGetUniformLocation(impl.program, i_name);
        if (location < 0) 
        {
            throw std::runtime_error("風向きの表示用シェーダーの設定が見つかりません。");
        }
        return location;
    };

    // ビュー行列、射影行列、原点、管理空間サイズをシェーダーに設定する
    glUniformMatrix4fv(uniform("view"), 1, GL_FALSE, view.data());
    glUniformMatrix4fv(uniform("projection"), 1, GL_FALSE, projection.data());
    glUniform3iv(uniform("origin"), 1, i_origin.data());
    glUniform3iv(uniform("size"), 1, i_size.data());
    // XYZ 成分のテクスチャをそれぞれ対応するシェーダー入力に結び付ける。
    constexpr auto names = std::array{"windX", "windY", "windZ"};
    for (auto axis = std::size_t{}; axis < i_textures.size(); ++axis)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(axis));
        glBindTexture(GL_TEXTURE_3D, i_textures[axis]);
        glBindSampler(static_cast<GLuint>(axis), impl.sampler);
        glUniform1i(uniform(names[axis]), static_cast<GLint>(axis));
    }
    
    // シェーダーが各セルの風速から 5 本の線分を作り、セルごとに描画する。
    glBindVertexArray(impl.vao);
    glDrawArraysInstanced(GL_LINES, 0, 10, static_cast<GLsizei>(count));
    if (glGetError() != GL_NO_ERROR) { throw std::runtime_error("風向きの矢印を描画できません。"); }
}

}
