/**
 * @file wind_arrow.vert
 * @brief 各セルの GPU 風速から矢印の頂点位置と色を計算する。
 * @details 3 枚の符号付き 16.16 固定小数点 3D テクスチャから XYZ 成分を読み、
 * インスタンス番号を X 最速のセル座標へ変換する。矢印は風向きに向け、風速に応じて長さを調整する。
 * 風がゼロのセルはクリップ空間外へ移動して描画しない。位置は view と projection で変換する。
 */
#version 430 core

// カメラ変換行列と管理空間の原点・寸法。
uniform mat4 view;
uniform mat4 projection;
uniform ivec3 origin;
uniform ivec3 size;

// 風速の X・Y・Z 成分を格納した符号付き固定小数点テクスチャ。
uniform isampler3D windX;
uniform isampler3D windY;
uniform isampler3D windZ;

// 同じセルの 10 頂点で共通の風速色を使う。
flat out vec3 windColor;

// 原点から風向きへ伸びる軸と、その先端の四方向の羽根を作る 5 本の線分。
const vec3 arrowVertices[10] = vec3[](
    vec3(-0.5, 0.0, 0.0), vec3(0.5, 0.0, 0.0),
    vec3(0.5, 0.0, 0.0), vec3(0.214285714, 0.171428571, 0.0),
    vec3(0.5, 0.0, 0.0), vec3(0.214285714, -0.171428571, 0.0),
    vec3(0.5, 0.0, 0.0), vec3(0.214285714, 0.0, 0.171428571),
    vec3(0.5, 0.0, 0.0), vec3(0.214285714, 0.0, -0.171428571)
);

void main()
{
    // インスタンス番号を、X が最速で増加する X・Y・Z のセル座標に戻す。
    int index = gl_InstanceID;
    ivec3 cell = ivec3(index % size.x, (index / size.x) % size.y, index / (size.x * size.y));

    // 3 軸のテクスチャから、このセルの符号付き 16.16 固定小数点風速を読む。
    ivec3 fixedWind = ivec3(texelFetch(windX, cell, 0).r, texelFetch(windY, cell, 0).r,
                            texelFetch(windZ, cell, 0).r);
    if (all(equal(fixedWind, ivec3(0))))
    {
        // 無風セルには画面上の色を与えず、全頂点をクリップ範囲外へ置く。
        windColor = vec3(0.0);
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    // 固定小数点を m/s に戻し、単位方向と矢印の長さを求める。
    vec3 wind = vec3(fixedWind) / 65536.0;
    float speed = length(wind);
    vec3 axis = wind / speed;

    // 風向きと平行にならない基準方向から、矢印断面の直交する二方向を作る。
    vec3 reference = abs(axis.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(axis, reference));
    vec3 other = cross(axis, side);

    // ローカルの矢印頂点を風向きに合わせ、セル中心を基準に配置する。
    vec3 local = arrowVertices[gl_VertexID];
    float arrowLength = 0.7 * min(speed / 32.7, 1.0);
    vec3 center = vec3(origin) + vec3(cell) + vec3(0.5);
    vec3 position = center + arrowLength * (axis * local.x + side * local.y + other * local.z);

    // 各風速成分をプレビューと同じ符号付き RGB 範囲へ変換する。
    windColor = (clamp(wind / 32.7, -1.0, 1.0) + 1.0) * 0.5;
    gl_Position = projection * view * vec4(position, 1.0);
}
