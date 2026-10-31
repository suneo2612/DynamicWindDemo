/**
 * @file wind_common.cpp
 * @brief 風の GPU 処理で共有する検証と OpenGL 操作。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "wind_common.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace demo::wind::simulation::detail
{
namespace windDefs = demo::wind::defs;

/**
 *  @brief GPU のエラーを読み取り、失敗を呼び出し側に伝える。
 */
void CheckGl(const char* i_stage)
{
    const auto error = glGetError();
    if (error != GL_NO_ERROR)
    {
        throw std::runtime_error(std::string(i_stage) + ": OpenGL エラー " + std::to_string(error));
    }
}

/** 
 *  @brief  コンピュートシェーダーをコンパイルしてプログラムに登録する。
 *  @param  i_source コンピュートシェーダーのソースコード。
 *  @return 作成されたプログラムの ID。
 *  @throws std::runtime_error コンパイルやリンクに失敗した場合。
 */
GLuint Compile(const char* i_source)
{
    // ComputeShader を作成する
    const auto shader = glCreateShader(GL_COMPUTE_SHADER);
    if (shader == 0) { throw std::runtime_error("風のシェーダーを作成できません。"); }
    glShaderSource(shader, 1, &i_source, nullptr);
    glCompileShader(shader);
    auto success = GLint{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        auto length = GLint{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("風のシェーダーをコンパイルできません: " + log);
    }

    // プログラムを作成する
    const auto program = glCreateProgram();
    if (program == 0)
    {
        glDeleteShader(shader);
        throw std::runtime_error("風のプログラムを作成できません。");
    }

    // シェーダーをプログラムにアタッチしてリンクする
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        auto length = GLint{};
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

        auto log = std::string(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("風のプログラムをリンクできません: " + log);
    }

    // プログラムのリンクに成功した場合は ID を返す
    return program;
}

/** 
 *  @brief 風速として利用可能な有限ベクトルか検証する。
 *  @param  i_wind 風速ベクトル。
 *  @throws std::invalid_argument 風速が有限値でないか 64 m/s を超える場合。
 */
void ValidateWind(const std::array<float, windDefs::kAxisCount>& i_wind)
{

    if (!std::all_of(i_wind.begin(),
                     i_wind.end(),
                     [](const auto i_value) 
                     {
                        return std::isfinite(i_value);
                     }) ||
        std::hypot(static_cast<double>(i_wind[0]), static_cast<double>(i_wind[1]),
                   static_cast<double>(i_wind[2])) > 64.0)
    {
        throw std::invalid_argument("風速は有限値かつ 64 m/s 以下にしてください。");
    }
}

/** 
 *  @brief  シェーダーで使用する設定（uniform）の項目の位置を取得する
 *  @param  i_program   シェーダープログラムの ID
 *  @param  i_name      設定項目の名前。
 *  @return 設定項目の位置
 *  @throws std::runtime_error 設定項目が見つからない場合
 */
GLint Uniform(const GLuint i_program, const char* i_name)
{
    const auto location = glGetUniformLocation(i_program, i_name);
    if (location < 0) { throw std::runtime_error(std::string("風の設定が見つかりません: ") + i_name); }
    return location;
}

/**
 *  @brief  占有率計算で 1 m ごとの位置を表現できる管理空間かを調べる。
 *  @param  i_origin    管理空間における原点の座標
 *  @param  i_size      管理空間におけるサイズ
 */
void ValidateOrigin(const std::array<int, windDefs::kAxisCount>& i_origin,
                    const std::array<int, windDefs::kAxisCount>& i_size)
{
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        if (i_origin[axis] < -16777216 ||
            static_cast<double>(i_origin[axis]) + i_size[axis] > 16777216.0)
        {
            throw std::invalid_argument("管理空間が占有率計算の座標範囲を超えています。");
        }
    }
}
ElapsedQuery::ElapsedQuery(const GLuint i_query)
{
    if (i_query != 0)
    {
        glBeginQuery(GL_TIME_ELAPSED, i_query);
        m_bActive = true;
    }
}

ElapsedQuery::~ElapsedQuery() { End(); }

void ElapsedQuery::End()
{
    if (m_bActive)
    {
        glEndQuery(GL_TIME_ELAPSED);
        m_bActive = false;
    }
}
}
