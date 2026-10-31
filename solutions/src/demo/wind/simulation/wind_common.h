/**
 * @file wind_common.h
 * @brief 風の GPU 処理で共有する検証と OpenGL 操作。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_COMMON_H
#define DYNAMIC_WIND_DEMO_DEMO_WIND_SIMULATION_WIND_COMMON_H

#include "../defs/wind_region_constants.h"
#include <GL/glew.h>
#include <array>

namespace demo::wind::simulation::detail
{
constexpr auto kScale = 65536.0F;

/**
 * @brief OpenGL エラーを例外として通知する。
 * @param i_stage エラーが発生した処理の名称。
 */
void CheckGl(const char* i_stage);

/**
 * @brief コンピュートプログラムを作成する。
 * @param i_source シェーダーのソース。
 * @return 呼び出し側が所有するプログラム名。
 */
GLuint Compile(const char* i_source);

/**
 * @brief 必須の uniform の位置を取得する。
 * @param i_program プログラム名。
 * @param i_name uniform 名。
 * @return uniform の位置。
 */
GLint Uniform(GLuint i_program, const char* i_name);

/**
 * @brief 風速ベクトルを検証する。
 * @param i_wind 検証する風速。
 */
void ValidateWind(const std::array<float, demo::wind::defs::kAxisCount>& i_wind);

/**
 * @brief 占有率計算で表現できる管理空間か検証する。
 * @param i_origin 管理空間の原点。
 * @param i_size 管理空間の寸法。
 */
void ValidateOrigin(const std::array<int, demo::wind::defs::kAxisCount>& i_origin,
                    const std::array<int, demo::wind::defs::kAxisCount>& i_size);

/**
 * @brief GPU 時間クエリの開始と終了を管理する。
 */
class ElapsedQuery final
{
public:
    /**
     * @brief 有効なクエリを開始する。0 の場合は計測しない。
     * @param i_query 経過時間クエリの ID。
     */
    explicit ElapsedQuery(GLuint i_query);
    ~ElapsedQuery();
    ElapsedQuery(const ElapsedQuery&) = delete;
    ElapsedQuery& operator=(const ElapsedQuery&) = delete;

    /**
     * @brief 開始済みのクエリを終了する。
     */
    void End();

private:
    bool m_bActive = false; // クエリの終了が必要か。
};
}
#endif
