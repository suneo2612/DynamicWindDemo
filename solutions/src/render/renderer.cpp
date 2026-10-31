/**
 * @file renderer.cpp
 * @brief OpenGL 資源の所有と静的モデルの描画を実装する。
 * @date 2026-10-28
 * @par 最終更新日 2026-10-29
 */
#include "renderer.h"
#include "shader_sources.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace render
{
namespace
{
struct Vertex
{
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 2> uv;
};

struct Batch
{
    GLuint vao = 0;
    GLuint buffer = 0;
    GLsizei count = 0;
    std::array<float, 16> node{};
    loader::MaterialData material;
    ~Batch()
    {
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
    }
};

void CheckGl(const char* i_stage)
{
    auto errors = std::string{};
    for (auto error = glGetError(); error != GL_NO_ERROR; error = glGetError())
    {
        if (!errors.empty()) { errors += ", "; }
        errors += std::to_string(error);
    }
    if (!errors.empty())
    {
        throw std::runtime_error(std::string(i_stage) + ": OpenGL エラー " + errors);
    }
}

GLuint Compile(const GLenum i_type, const char* i_source)
{
    const auto shader = glCreateShader(i_type);
    if (shader == 0) { throw std::runtime_error("シェーダーを作成できません。"); }
    glShaderSource(shader, 1, &i_source, nullptr);
    glCompileShader(shader);
    auto success = GLint{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE)
    {
        auto length = GLint{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("シェーダーのコンパイルに失敗しました: " + log);
    }
    return shader;
}

struct Shader final
{
    explicit Shader(const GLenum i_type, const char* i_source) : id(Compile(i_type, i_source)) {}
    ~Shader() { glDeleteShader(id); }
    GLuint id;
};

GLint Uniform(const GLuint i_program, const char* i_name)
{
    const auto location = glGetUniformLocation(i_program, i_name);
    if (location < 0)
    {
        throw std::runtime_error(std::string("シェーダーの設定が見つかりません: ") + i_name);
    }
    return location;
}
}

struct Model::Impl
{
    std::vector<GLuint> textures;
    std::vector<std::unique_ptr<Batch>> batches;
    ~Impl()
    {
        for (const auto texture : textures) { glDeleteTextures(1, &texture); }
    }
};

Model::Model(const loader::ModelData& i_data) : m_instImpl(std::make_unique<Impl>())
{
    if (i_data.meshes.empty() || i_data.materials.empty())
    {
        throw std::runtime_error("描画可能なメッシュまたはマテリアルがありません。");
    }
    m_instImpl->textures.reserve(i_data.textures.size());
    for (const auto& texture : i_data.textures)
    {
        const auto width = static_cast<std::size_t>(texture.width);
        const auto height = static_cast<std::size_t>(texture.height);
        if (width == 0 || height == 0 ||
            width > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
            height > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
            width > std::numeric_limits<std::size_t>::max() / height / 4 ||
            texture.rgba8.size() != width * height * 4)
        {
            throw std::runtime_error("テクスチャのサイズまたは画像データが不正です。");
        }
        auto id = GLuint{};
        glGenTextures(1, &id);
        if (id == 0) { throw std::runtime_error("テクスチャを作成できません。"); }
        m_instImpl->textures.push_back(id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba8.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        CheckGl("テクスチャの登録");
    }
    for (const auto& mesh : i_data.meshes)
    {
        if (mesh.indices.empty() || mesh.indices.size() % 3 != 0 ||
            mesh.triangle_material_indices.size() != mesh.indices.size() / 3 ||
            mesh.normals.size() != mesh.positions.size())
        {
            throw std::runtime_error("メッシュの頂点またはマテリアル情報が不正です: " + mesh.name);
        }
        if (!std::all_of(mesh.node_transform.begin(), mesh.node_transform.end(),
                         [](const float i_value) { return std::isfinite(i_value); }))
        {
            throw std::runtime_error("メッシュの変換行列に有限でない値があります: " + mesh.name);
        }
        auto groups = std::map<std::uint32_t, std::vector<Vertex>>{};
        for (auto triangle = std::size_t{}; triangle < mesh.triangle_material_indices.size(); ++triangle)
        {
            const auto materialIndex = mesh.triangle_material_indices[triangle];
            if (materialIndex >= i_data.materials.size())
            {
                throw std::runtime_error("マテリアルの参照が不正です: " + mesh.name);
            }
            const auto& material = i_data.materials[materialIndex];
            if (!std::isfinite(material.roughness) || !std::isfinite(material.metallic) ||
                !std::all_of(material.base_color.begin(), material.base_color.end(),
                             [](const float i_value) { return std::isfinite(i_value); }))
            {
                throw std::runtime_error("マテリアルに有限でない値があります: " + material.name);
            }
            auto uvIndex = std::size_t{};
            if (material.base_color_texture)
            {
                if (*material.base_color_texture >= i_data.textures.size() || mesh.uv_sets.empty())
                {
                    throw std::runtime_error("テクスチャまたは UV がありません: " + mesh.name);
                }
                if (!material.base_color_uv_set.empty())
                {
                    const auto found = std::find(mesh.uv_set_names.begin(), mesh.uv_set_names.end(),
                                                 material.base_color_uv_set);
                    if (found == mesh.uv_set_names.end())
                    {
                        throw std::runtime_error("指定された UV セットがありません: " + material.base_color_uv_set);
                    }
                    uvIndex = static_cast<std::size_t>(found - mesh.uv_set_names.begin());
                }
                if (uvIndex >= mesh.uv_sets.size() || mesh.uv_sets[uvIndex].size() != mesh.positions.size())
                {
                    throw std::runtime_error("UV セットの頂点数が不正です: " + mesh.name);
                }
            }
            auto& vertices = groups[materialIndex];
            for (auto corner = std::size_t{}; corner < 3; ++corner)
            {
                const auto index = mesh.indices[triangle * 3 + corner];
                if (index >= mesh.positions.size())
                {
                    throw std::runtime_error("頂点インデックスが範囲外です: " + mesh.name);
                }
                const auto uv = material.base_color_texture ? mesh.uv_sets[uvIndex][index] : std::array<float, 2>{};
                const auto finite = [](const auto& i_values)
                {
                    return std::all_of(i_values.begin(), i_values.end(),
                                       [](const float i_value) { return std::isfinite(i_value); });
                };
                if (!finite(mesh.positions[index]) || !finite(mesh.normals[index]) || !finite(uv))
                {
                    throw std::runtime_error("頂点属性に有限でない値があります: " + mesh.name);
                }
                vertices.push_back(Vertex{mesh.positions[index], mesh.normals[index], uv});
            }
        }
        for (const auto& [materialIndex, vertices] : groups)
        {
            if (vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
                vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(Vertex))
            {
                throw std::runtime_error("描画する頂点数が上限を超えています。");
            }
            auto batch = std::make_unique<Batch>();
            batch->node = mesh.node_transform;
            batch->material = i_data.materials[materialIndex];
            batch->count = static_cast<GLsizei>(vertices.size());
            glGenVertexArrays(1, &batch->vao);
            glGenBuffers(1, &batch->buffer);
            if (!batch->vao || !batch->buffer) { throw std::runtime_error("頂点バッファを作成できません。"); }
            glBindVertexArray(batch->vao);
            glBindBuffer(GL_ARRAY_BUFFER, batch->buffer);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                         vertices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  reinterpret_cast<const void*>(offsetof(Vertex, position)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  reinterpret_cast<const void*>(offsetof(Vertex, normal)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  reinterpret_cast<const void*>(offsetof(Vertex, uv)));
            CheckGl("頂点バッファの登録");
            m_instImpl->batches.push_back(std::move(batch));
        }
    }
}

Model::~Model() = default;

/**
 * @brief 描画プログラムの終了担保
 */
struct Renderer::Impl
{
    GLuint program = 0;
    ~Impl() { glDeleteProgram(program); }   // 描画プログラムの終了を担保する
};

/**
 * @brief Rendererの実装クラス
 */
Renderer::Renderer() : 
    m_instImpl(std::make_unique<Impl>())
{
    // 頂点シェーダーとフラグメントシェーダーを作成し、プログラムにリンクする。
    const auto vertex   =   Shader(GL_VERTEX_SHADER, shaders::modelVertex);
    const auto fragment =   Shader(GL_FRAGMENT_SHADER, shaders::modelFragment);

    // プログラムを作成し、シェーダーをリンクする。
    m_instImpl->program = glCreateProgram();
    if (!m_instImpl->program) { throw std::runtime_error("描画プログラムを作成できません。"); }

    // シェーダーをプログラムにアタッチ
    glAttachShader(m_instImpl->program, vertex.id);
    glAttachShader(m_instImpl->program, fragment.id);
    glLinkProgram(m_instImpl->program);
    auto success = GLint{};

    // プログラムのリンク状態を取得してチェックする。
    glGetProgramiv(m_instImpl->program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE)
    {
        // リンクに失敗した場合はログを取得して例外を投げる。
        auto length = GLint{};
        glGetProgramiv(m_instImpl->program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(m_instImpl->program, length, nullptr, log.data());
        throw std::runtime_error("描画プログラムのリンクに失敗しました: " + log);
    }
    CheckGl("描画プログラムの作成");
}

/**
 * @brief Rendererのデストラクタ
 */
Renderer::~Renderer() = default;

/**
 * @brief 指定したモデルを Entity のワールド変換で描画する。
 * @param i_model 描画するモデル。
 * @param i_world 同期済みの Entity ワールド行列。
 * @param i_camera カメラ。
 * @param i_aspect 画面の縦横比。
 * @param i_lighting 照明設定。
 */
void Renderer::Draw(const Model&                    i_model,
                    const general::math::Matrix4&   i_world,
                    const Camera&                   i_camera,
                    const float                     i_aspect,
                    const Lighting&                 i_lighting) const
{
    // 光の方向が有限かつゼロでないことを確認する。
    const auto length = i_lighting.direction.Length();
    if (!std::isfinite(length) || length < 1e-8F)
    {
        throw std::invalid_argument("光の方向には有限のゼロでないベクトルを指定してください。");
    }

    // シェーダーを選択し、ワールド・ビュー・投影行列とカメラ位置を設定する。
    const auto program = m_instImpl->program;
    const auto view = i_camera.View();
    const auto projection = Camera::Projection(i_aspect);
    const auto position = i_camera.Position();
    glUseProgram(program);
    glUniformMatrix4fv(Uniform(program, "world"), 1, GL_FALSE, i_world.Data().data());
    glUniformMatrix4fv(Uniform(program, "view"), 1, GL_FALSE, view.data());
    glUniformMatrix4fv(Uniform(program, "projection"), 1, GL_FALSE, projection.data());
    glUniform3f(Uniform(program, "cameraPosition"), position.x, position.y, position.z);

    // 照明設定を渡し、マテリアル用テクスチャの参照先を設定する。
    glUniform3f(Uniform(program, "lightDirection"), i_lighting.direction.x, i_lighting.direction.y, i_lighting.direction.z);
    glUniform1i(Uniform(program, "lightingEnabled"), i_lighting.enabled ? 1 : 0);
    glUniform1i(Uniform(program, "baseTexture"), 0);
    glActiveTexture(GL_TEXTURE0);

    // 各バッチのノード変換と材質を設定して描画する。
    for (const auto& batch : i_model.m_instImpl->batches)
    {
        const auto& material = batch->material;
        glUniformMatrix4fv(Uniform(program, "node"), 1, GL_FALSE, batch->node.data());      // ノードの変換行列をシェーダーに渡す
        glUniform3fv(Uniform(program, "baseColor"), 1, material.base_color.data());         // マテリアルの基本色をシェーダーに渡す
        glUniform1f(Uniform(program, "roughness"), material.roughness);                     // 粗さをシェーダーに渡す
        glUniform1f(Uniform(program, "metallic"), material.metallic);                       // 金属度をシェーダーに渡す
        glUniform1i(Uniform(program, "hasTexture"), material.base_color_texture ? 1 : 0);   // テクスチャの有無をシェーダーに渡す

        // 材質のテクスチャと頂点配列を選択し、バッチを三角形として描画する。
        glBindTexture(GL_TEXTURE_2D, material.base_color_texture
            ? i_model.m_instImpl->textures[*material.base_color_texture] : 0);   // 材質のテクスチャをバインドする
        glBindVertexArray(batch->vao);                                           // 頂点配列をバインドする
        glDrawArrays(GL_TRIANGLES, 0, batch->count);                             // バッチを三角形として描画する
    }

    // 描画中に発生したOpenGLエラーを確認する。
    CheckGl("モデルの描画");
}
}
