/**
 * @file wind_debug.frag
 * @brief 線分描画のフラグメントシェーダー。
 * @details 頂点シェーダーで設定された一様色を、不透明な画素色として出力する。
 */
#version 430 core

// 線分全体に共通する RGB 色。
uniform vec3 lineColor;
out vec4 color;

void main()
{
    // 指定色を不透明な画素色として出力する。
    color = vec4(lineColor, 1.0);
}
