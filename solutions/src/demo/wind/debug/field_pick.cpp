/**
 * @file field_pick.cpp
 * @brief 画面座標とモデルの三角形からフィールド表面を選ぶ。
 * @details ウィンドウ座標を描画画素に対応するワールド空間の半直線へ変換し、
 * ノード変換済みモデル三角形と Entity のワールド変換を使って最も手前の交点を求める。
 * 交差判定では三角形の表裏を区別せず、カメラの近・遠クリップ範囲にある命中だけを返す。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-31
 */
#include "field_pick.h"
#include "../../../render/defs/camera_constants.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace demo::wind::debug
{

namespace
{

/**
 * @brief 三次元位置・ベクトルを倍精度で計算するための型。
 * @details モデルの頂点や行列は float だが、交差判定の積和誤差を抑えるため内部計算を double で行う。
 */
using Point = std::array<double, defs::kAxisCount>;

/**
 * @brief 二つの三次元ベクトルの差を求める。
 * @param i_left 被減数。
 * @param i_right 減数。
 * @return 各成分について i_left - i_right を格納したベクトル。
 */
Point Subtract(const Point& i_left, const Point& i_right)
{
    // 対応する XYZ 成分をそれぞれ引き、辺ベクトルや原点からの差を得る。
    return {i_left[defs::kAxisXIndex] - i_right[defs::kAxisXIndex],
            i_left[defs::kAxisYIndex] - i_right[defs::kAxisYIndex],
            i_left[defs::kAxisZIndex] - i_right[defs::kAxisZIndex]};
}

/**
 * @brief 二つのベクトルの内積を求める。
 * @param i_left 左側のベクトル。
 * @param i_right 右側のベクトル。
 * @return 対応する成分の積を合計したスカラー値。
 */
double Dot(const Point& i_left, const Point& i_right)
{
    // 内積は方向の平行度や、ベクトルを辺方向へ射影した量の計算に使う。
    return i_left[defs::kAxisXIndex] * i_right[defs::kAxisXIndex] +
           i_left[defs::kAxisYIndex] * i_right[defs::kAxisYIndex] +
           i_left[defs::kAxisZIndex] * i_right[defs::kAxisZIndex];
}

/**
 * @brief 二つのベクトルの外積を求める。
 * @param i_left 左側のベクトル。
 * @param i_right 右側のベクトル。
 * @return 両方に直交し、右手系の向きを持つベクトル。
 */
Point Cross(const Point& i_left, const Point& i_right)
{
    // 成分の積の差から、二つの入力に垂直なベクトルを作る。
    return {i_left[defs::kAxisYIndex] * i_right[defs::kAxisZIndex] - i_left[defs::kAxisZIndex] * i_right[defs::kAxisYIndex],
            i_left[defs::kAxisZIndex] * i_right[defs::kAxisXIndex] - i_left[defs::kAxisXIndex] * i_right[defs::kAxisZIndex],
            i_left[defs::kAxisXIndex] * i_right[defs::kAxisYIndex] - i_left[defs::kAxisYIndex] * i_right[defs::kAxisXIndex]};
}

/**
 * @brief 列優先の 4 行 4 列行列で位置を変換する。
 * @param i_matrix 列優先で格納された変換行列。
 * @param i_point 変換前の三次元位置。
 * @return 平行移動を含む変換後の三次元位置。
 * @details 位置の同次成分を 1 として変換する。対象行列はモデル・ワールド変換などのアフィン変換を想定し、
 * 変換後の同次成分による除算は行わない。
 */
Point Transform(const std::array<float, 16>& i_matrix, const Point& i_point)
{
    // 列優先行列の各行から XYZ と平行移動の成分を計算する。
    return {i_matrix[0] * i_point[defs::kAxisXIndex] + 
            i_matrix[4] * i_point[defs::kAxisYIndex] +
            i_matrix[8] * i_point[defs::kAxisZIndex] + i_matrix[12],
            i_matrix[1] * i_point[defs::kAxisXIndex] + 
            i_matrix[5] * i_point[defs::kAxisYIndex] +
            i_matrix[9] * i_point[defs::kAxisZIndex] + i_matrix[13],
            i_matrix[2] * i_point[defs::kAxisXIndex] +
            i_matrix[6] * i_point[defs::kAxisYIndex] +
            i_matrix[10] * i_point[defs::kAxisZIndex] + i_matrix[14]};
}
}

/**
 * @brief 画面上の位置を通る、カメラの描画範囲内のワールド空間半直線を作る。
 * @param i_camera 描画に使うカメラ。視点・向き・投影条件を取得する。
 * @param i_mouse ウィンドウ左上を原点とする論理画面上のマウス座標。
 * @param i_windowSize マウス座標の基準となるウィンドウの論理幅と高さ。
 * @param i_framebufferSize 実際に描画するフレームバッファーの幅と高さ。
 * @return マウスがウィンドウ内なら半直線、ウィンドウ外なら空。
 * @throws std::invalid_argument マウス座標が非有限値、またはウィンドウ・フレームバッファーの寸法が正でない場合。
 * @details 論理画面座標をフレームバッファーの画素へ対応付け、画素中心を通る方向を計算する。
 * 半直線の方向は正規化され、近・遠距離はカメラのクリップ面に対応する。
 */
std::optional<PickRay> ScreenPickRay(const render::Camera& i_camera, const std::array<double, 2>& i_mouse,
                                     const std::array<int, 2>& i_windowSize,
                                     const std::array<int, 2>& i_framebufferSize)
{
    // 後続の座標変換で除算するため、入力座標が有限で画面寸法が正であることを先に確認する。
    if (!std::isfinite(i_mouse[0]) ||
        !std::isfinite(i_mouse[1]) ||
        i_windowSize[0] <= 0 ||
        i_windowSize[1] <= 0 ||
        i_framebufferSize[0] <= 0 ||
        i_framebufferSize[1] <= 0)
    {
        throw std::invalid_argument("クリック座標または画面サイズが不正です。");
    }

    // ウィンドウの右端・下端は座標範囲に含めない。範囲外のクリックは異常ではなく、命中対象なしとして返す。
    if (i_mouse[0] < 0.0 ||
        i_mouse[1] < 0.0 ||
        i_mouse[0] >= i_windowSize[0]
        || i_mouse[1] >= i_windowSize[1])
    {
        return std::nullopt;
    }

    // 論理座標から描画画素へ換算する。Y 軸はウィンドウ座標が上向き、フレームバッファーが下向きのため反転する。
    const auto pixelX   =   std::floor(i_mouse[0] * i_framebufferSize[0] / i_windowSize[0]);
    const auto pixelY   =   i_framebufferSize[1] - 1.0 - std::floor(i_mouse[1] * i_framebufferSize[1] / i_windowSize[1]);

    // 画素の中心を NDC の -1～1 に写す。半画素分を加えることで画素の左上端ではなく中心を通す。
    const auto x = 2.0 * (pixelX + 0.5) / i_framebufferSize[0] - 1.0;
    const auto y = 2.0 * (pixelY + 0.5) / i_framebufferSize[1] - 1.0;

    // 描画時と同じフレームバッファの縦横比で投影行列を求め、カメラ基底と組み合わせてワールド方向へ戻す。
    const auto aspect       =   static_cast<float>(i_framebufferSize[0]) / static_cast<float>(i_framebufferSize[1]);
    const auto projection   =   render::Camera::Projection(aspect);
    const auto view         =   i_camera.View();
    const auto forward      =   i_camera.Forward();
    auto direction = Point
    {
        forward.x + view[0] * x / projection[0] + view[1] * y / projection[5],
        forward.y + view[4] * x / projection[0] + view[5] * y / projection[5],
        forward.z + view[8] * x / projection[0] + view[9] * y / projection[5]
    };

    // 交差距離をワールド単位で扱えるよう、画面位置から得た方向を長さ 1 にする。
    const auto length = std::hypot(direction[defs::kAxisXIndex], direction[defs::kAxisYIndex],
                                   direction[defs::kAxisZIndex]);
    for (auto& component : direction) { component /= length; }

    // クリップ面の距離はカメラ前方に沿った距離なので、斜め方向の半直線距離へ射影で換算する。
    // 正規化した方向とカメラ前方の内積が換算係数となる。
    const auto forwardDistance  =   Dot(direction, {forward.x, forward.y, forward.z});
    const auto position         =   i_camera.Position();
    return PickRay{{position.x, position.y, position.z}, direction,
                   render::defs::kCameraNearPlane / forwardDistance,
                   render::defs::kCameraFarPlane / forwardDistance};
}

/**
 * @brief モデルの全メッシュから三角形を抽出し、各メッシュのノード変換を適用して保持する。
 * @param i_data 頂点位置、三角形インデックス、メッシュごとのノード変換を含むモデルデータ。
 * @throws std::invalid_argument インデックス数が三角形単位でない、ノード変換が非有限値、
 * 頂点参照が範囲外、または頂点位置が非有限値の場合。
 * @details 頂点配列の所有権は持たず、交差判定に必要な三角形位置だけを倍精度で保存する。
 */
FieldSurface::FieldSurface(const loader::ModelData& i_data)
{
    // メッシュごとにインデックスとノード変換を検証し、変換済み三角形へまとめる。
    for (const auto& mesh : i_data.meshes)
    {
        // インデックス列は三頂点単位でなければならず、変換行列の非有限値は位置計算を壊すため拒否する。
        if (mesh.indices.size() % 3 != 0 ||
            !std::all_of(mesh.node_transform.begin(), mesh.node_transform.end(),
                         [](const float i_value) { return std::isfinite(i_value); }))
        {
            throw std::invalid_argument("フィールドの三角形またはノード変換が不正です。");
        }

        // インデックスを三つずつ読み、独立した三角形として保持する。
        for (auto index = std::size_t{}; index < mesh.indices.size(); index += 3)
        {
            auto triangle = std::array<Point, 3>{};

            // 三頂点それぞれの参照を検査してから、メッシュのノード変換を適用する。
            for (auto corner = std::size_t{}; corner < 3; ++corner)
            {
                const auto vertexIndex = mesh.indices[index + corner];
                if (vertexIndex >= mesh.positions.size())
                {
                    throw std::invalid_argument("フィールドの頂点参照が不正です。");
                }

                // NaN や無限値を変換後の三角形へ持ち込まないよう、元の頂点座標を確認する。
                const auto& position = mesh.positions[vertexIndex];
                if (!std::all_of(position.begin(), position.end(), [](const float i_value) { return std::isfinite(i_value); }))
                {
                    throw std::invalid_argument("フィールドの頂点が不正です。");
                }

                // 描画時のメッシュ配置を交差判定にも反映し、入力 float は倍精度位置へ変換して保持する。
                triangle[corner] = Transform(mesh.node_transform,
                                             {position[defs::kAxisXIndex], position[defs::kAxisYIndex],
                                              position[defs::kAxisZIndex]});
            }
            m_instTriangles.push_back(triangle);
        }
    }
}

/**
 * @brief ワールド変換後の全三角形に半直線を当て、最も手前の命中を求める。
 * @param i_ray 始点・単位方向・有効距離範囲を持つワールド空間の半直線。
 * @param i_world 対象 Entity に適用するワールド変換行列。
 * @return 近・遠距離の範囲内で命中した場合はワールド位置と始点からの距離、命中しない場合は空。
 * @throws std::invalid_argument 半直線の成分、距離範囲、または方向の長さが不正な場合。
 * @throws std::overflow_error 命中位置が float で表現できない場合。
 * @details 両面を対象とする Möller–Trumbore 法で各三角形を判定する。三角形の表裏は区別せず、
 * 近クリップ距離以上、遠クリップ距離以下にある最小距離の交点を返す。
 */
std::optional<PickHit> FieldSurface::Intersect(const PickRay& i_ray, const general::math::Matrix4& i_world) const
{
    // 入力全体の有限性を共通判定し、後続の交差計算に NaN や無限値が入るのを防ぐ。
    const auto finite = [](const double i_value) { return std::isfinite(i_value); };

    // 入力の有限性をチェック
    const auto hasFiniteCoordinates =   std::all_of(i_ray.origin.begin(), i_ray.origin.end(), finite) &&
                                        std::all_of(i_ray.direction.begin(), i_ray.direction.end(), finite);

    // 入力の距離範囲が有効かチェック
    const auto hasValidDistanceRange    =   finite(i_ray.nearDistance) &&               // nearDistance が有限か
                                            finite(i_ray.farDistance) &&                // farDistance  が有限か
                                            i_ray.nearDistance >= 0.0 &&                // nearDistance が 0 以上か
                                            i_ray.farDistance >= i_ray.nearDistance;    // farDistance  が nearDistance 以上か

    // 入力の方向ベクトルの長さが小さすぎても不正とみなす
    const auto directionLength = std::hypot(i_ray.direction[defs::kAxisXIndex], 
                                            i_ray.direction[defs::kAxisYIndex],
                                            i_ray.direction[defs::kAxisZIndex]);
    if (!hasFiniteCoordinates || !hasValidDistanceRange || std::abs(directionLength - 1.0) > 1e-6)
    {
        throw std::invalid_argument("表面選択の半直線が不正です。");
    }

    // 遠クリップ距離を初期上限として、これより近い交点だけを順次採用する。
    // hit は距離が上限と同じだけの候補がない場合に、初期値を命中と誤認しないために別管理する。
    auto nearest    =   i_ray.farDistance;
    auto hit        =   false;

    // 保存済み三角形を Entity のワールド座標へ移し、半直線との交差を調べる。
    for (const auto& source : m_instTriangles)
    {
        // ノード変換は構築時に適用済み。ここでは描画対象 Entity のワールド変換を重ねる。
        const auto first    =   Transform(i_world.Data(), source[0]);
        const auto second   =   Transform(i_world.Data(), source[1]);
        const auto third    =   Transform(i_world.Data(), source[2]);

        // Möller–Trumbore 法で使う二辺と、方向・二辺の外積を計算する。
        const auto edge1        =   Subtract(second, first);
        const auto edge2        =   Subtract(third, first);
        const auto cross        =   Cross(i_ray.direction, edge2);
        const auto determinant  =   Dot(edge1, cross);

        // 面積がゼロの三角形と、半直線がほぼ平行な三角形は交差判定の対象外とする。
        // 固定の閾値ではなく辺の長さの積で比較し、三角形の大きさに応じた判定にする。
        const auto scale = std::hypot(edge1[defs::kAxisXIndex], edge1[defs::kAxisYIndex],
                                      edge1[defs::kAxisZIndex]) *
                           std::hypot(edge2[defs::kAxisXIndex], edge2[defs::kAxisYIndex],
                                      edge2[defs::kAxisZIndex]);
        if (scale == 0.0 || std::abs(determinant) <= scale * 1e-12) { continue; }   // レイと三角形がほぼ平行な場合は交差判定の対象外
        const auto inverse = 1.0 / determinant;

        // 始点から三角形頂点までの差から、最初の頂点に対する重心係数 u を求める。
        // determinant の符号を残すため、表面の向きにかかわらず両面を判定できる。
        const auto offset   =   Subtract(i_ray.origin, first);
        const auto u        =   Dot(offset, cross) * inverse;
        if (u < -1e-10 || u > 1.0 + 1e-10) { continue; }        // u が範囲外なら次の三角形へ

        // もう一つの重心係数 v を求める。u + v が 1 を超える点は三角形の外側。
        const auto offsetCross = Cross(offset, edge1);
        const auto v = Dot(i_ray.direction, offsetCross) * inverse;
        if (v < -1e-10 || u + v > 1.0 + 1e-10) { continue; }    // v が範囲外なら次の三角形へ

        // 半直線方向が単位長さなので、ここで得る媒介変数は始点からのワールド距離に一致する。
        const auto distance = Dot(edge2, offsetCross) * inverse;

        // 近クリップ面より手前を除外し、これまでの候補より近い交点だけを記録する。
        // nearest の初期値が遠クリップ距離なので、それより奥の交点も採用されない。
        if (distance >= i_ray.nearDistance && distance <= nearest)
        {
            nearest = distance;
            hit = true;
        }
    }
    if (!hit) { return std::nullopt; }

    // 最短距離を使って命中位置を復元し、呼び出し側の描画・発生器 API が使う float 配列へ格納する。
    auto result     =   PickHit{};
    result.distance =   nearest;
    for (auto axis = std::size_t{}; axis < defs::kAxisCount; ++axis)
    {
        // 各軸ごとに命中位置を計算する。
        const auto value = i_ray.origin[axis] + i_ray.direction[axis] * nearest;

        // 結果型の表現範囲を超える値を変換すると不定な位置になるため、変換前に失敗として通知する。
        if (!finite(value) || std::abs(value) > std::numeric_limits<float>::max())
        {
            throw std::overflow_error("フィールドの命中位置が表現範囲を超えています。");
        }
        result.position[axis] = static_cast<float>(value);
    }
    return result;
}

}   // namespace demo::wind::debug
