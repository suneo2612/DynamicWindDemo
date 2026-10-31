/**
 * @file wind_arrow.frag
 * @brief 風向き矢印の頂点シェーダーから受け取った色を画面へ出力する。
 * @details RGB は風速の XYZ 成分を -32.7～32.7 m/s の範囲で符号付き色に変換した値。
 * アルファ値は 1.0 とし、不透明な矢印として描画する。
 */
#version 430 core

// 頂点シェーダーでセル単位に決めた風速色。flat により線分内で補間しない。
flat in vec3 windColor;
out vec4 color;

void main()
{
    // 風速色を不透明な画素色として出力する。
    color = vec4(windColor, 1.0);
}
