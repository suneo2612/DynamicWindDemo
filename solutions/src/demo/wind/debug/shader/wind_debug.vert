/**
 * @file wind_debug.vert
 * @brief 線分描画用のワールド座標をカメラ空間へ変換する。
 * @details CPU が用意した位置を view と projection の順に変換し、ラスタライズ用のクリップ座標を出力する。
 */
#version 430 core

// 線分頂点のワールド座標。
layout(location = 0) in vec3 position;

// ワールド座標を視点空間、続いてクリップ空間へ変換する行列。
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // ワールド座標を透視投影して描画位置を決める。
    gl_Position = projection * view * vec4(position, 1.0);
}
