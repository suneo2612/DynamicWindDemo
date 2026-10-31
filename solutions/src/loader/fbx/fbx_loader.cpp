/**
 * @file fbx_loader.cpp
 * @brief FBX の静的モデルとテクスチャを読み込む。
 * @details ufbx のシーンから描画用の頂点属性、三角形、マテリアル、RGBA8 テクスチャを作成する。
 * @author Ryoma Seita
 * @date 2026-10-27
 * @par 最終更新日 2026-10-31
 */

#include "fbx_loader.h"

#include "../../general/util/file_util.h"
#include "../../general/util/path_util.h"
#include "../../general/util/string_util.h"

#include <ufbx.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace loader::fbx
{

/**
 * @brief 画像バイト列を RGBA8 テクスチャへデコードする。
 * @param i_bytes 画像データ。
 * @param i_source_name 画像の識別名。
 * @param o_texture デコードしたテクスチャ。
 * @param o_error デコード失敗時の説明。
 * @return デコードに成功した場合は true。
 */
bool DecodeImage(
    const std::vector<std::uint8_t>& i_bytes,
    const std::string& i_source_name,
    TextureData& o_texture,
    std::string& o_error)
{
    if (i_bytes.empty() ||
        i_bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        o_error = "画像データのサイズが不正です: " + i_source_name;
        return false;
    }

    // 画像をデコードして RGBA8 の画素データを取得する。
    int width   =   0;
    int height  =   0;
    using ImagePtr = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    auto pixels = ImagePtr(
        // メモリ上の画像データからデコードして RGBA8 のピクセルデータを取得する。
        stbi_load_from_memory(
            i_bytes.data(),
            static_cast<int>(i_bytes.size()),
            &width,
            &height,
            nullptr,
            STBI_rgb_alpha),
        stbi_image_free);
    if (!pixels)
    {
        const auto reason = stbi_failure_reason();
        o_error = "画像データをデコードできません: " + i_source_name;
        if (reason != nullptr)
        {
            o_error += "。詳細: ";
            o_error += reason;
        }
        return false;
    }

    // RGBA の画素数を計算する前に寸法を検査し、サイズ計算のオーバーフローを防ぐ。
    constexpr auto kBytesPerPixel = std::size_t{ 4 };
    if (width <= 0 ||
        height <= 0 ||
        static_cast<std::size_t>(width) >
            std::numeric_limits<std::size_t>::max() / kBytesPerPixel /
                static_cast<std::size_t>(height))
    {
        o_error = "画像の寸法が不正です: " + i_source_name;
        return false;
    }

    // ピクセルデータのバイト数を計算する。
    const auto pixelBytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel;
    // デコーダーが所有する画素をモデル側の配列へコピーし、解放後も保持できるようにする。
    o_texture.width     =   static_cast<std::uint32_t>(width);
    o_texture.height    =   static_cast<std::uint32_t>(height);
    // デコーダーから取得したピクセルデータをコピーする
    o_texture.rgba8.assign(pixels.get(), pixels.get() + pixelBytes);
    o_texture.source_path = i_source_name;
    return true;
}

/**
 * @brief FBX が参照する外部テクスチャを相対パスから読み込む。
 * @param i_texture FBX のテクスチャ情報。
 * @param i_model_directory FBX ファイルがあるディレクトリ。
 * @param o_texture 読み込んだテクスチャ。
 * @param o_error 読み込み失敗時の説明。
 * @return 読み込みに成功した場合は true。
 * @details 外部ファイルは FBX の相対パスとして解決し、絶対パスの参照は受け付けない。
 */
bool LoadTextureFile(
    const ufbx_texture& i_texture,
    const std::filesystem::path& i_model_directory,
    TextureData& o_texture,
    std::string& o_error)
{
    // FBX が相対パスとして保持している参照を優先して取得する。
    auto sourcePath = general::util::ToString(
        i_texture.relative_filename.data,
        i_texture.relative_filename.length);
    if (sourcePath.empty())
    {
        // 相対パスがない場合は、FBX のファイル名フィールドを代わりに調べる。
        sourcePath = general::util::ToString(
            i_texture.filename.data,
            i_texture.filename.length);
    }
    if (sourcePath.empty())
    {
        o_error = "テクスチャの参照パスが FBX にありません: " +
            general::util::ToString(i_texture.name.data, i_texture.name.length);
        return false;
    }

    // FBX 内の UTF-8 パスを OS のファイルシステムで扱うパス型へ変換する。
    const auto relativePath = general::util::PathFromUtf8(sourcePath);
    if (relativePath.is_absolute() || relativePath.has_root_name())
    {
        // テクスチャは FBX の配置場所を基準とする相対パスだけを許可する。
        o_error = "絶対パスのテクスチャ参照には対応していません: " + sourcePath;
        return false;
    }

    // FBX の配置先を正規化し、相対パスの基準ディレクトリを確定する。
    std::error_code fileSystemError;
    const auto basePath = std::filesystem::weakly_canonical(i_model_directory, fileSystemError);
    if (fileSystemError)
    {
        // error_code を使う overload の失敗理由を呼び出し側へ文字列で返す。
        o_error = "FBX のディレクトリを解決できません: " +
            general::util::PathToUtf8(i_model_directory);
        return false;
    }

    // FBX のディレクトリと参照パスを結合し、実際に読み込むパスを正規化する。
    const auto resolvedPath = std::filesystem::weakly_canonical(basePath / relativePath, fileSystemError);
    if (fileSystemError)
    {
        o_error = "テクスチャのパスを解決できません: " + sourcePath;
        return false;
    }

    // 画像ファイルをバイト列として読み込み、失敗理由はそのまま呼び出し側へ渡す。
    std::vector<std::uint8_t> imageBytes;
    if (!general::util::ReadFile(resolvedPath, imageBytes, o_error))
    {
        return false;
    }

    // テクスチャ名を保持し、読み込んだバイト列を RGBA8 画像へデコードする。
    o_texture.name = general::util::ToString(i_texture.name.data, i_texture.name.length);
    return DecodeImage(imageBytes, sourcePath, o_texture, o_error);
}

/**
 * @brief テクスチャ情報から画像を取得し、重複を避けてモデルへ追加する。
 * @param i_texture FBX のテクスチャ。
 * @param i_model_directory FBX ファイルがあるディレクトリ。
 * @param io_model 読み込み中のモデル。
 * @param io_texture_indices ufbx テクスチャとモデル内番号の対応表。
 * @param o_index モデル内テクスチャ番号。
 * @param o_error 失敗時の説明。
 * @return 追加または既存テクスチャの取得に成功した場合は true。
 */
bool GetOrAddTexture(
    const ufbx_texture* i_texture,
    const std::filesystem::path& i_model_directory,
    ModelData& io_model,
    std::unordered_map<const ufbx_texture*, std::uint32_t>& io_texture_indices,
    std::uint32_t& o_index,
    std::string& o_error)
{
    if (i_texture == nullptr)
    {
        o_error = "マテリアルのテクスチャ参照が不正です。";
        return false;
    }

    // 指定したポインタのテクスチャが既に追加されているか確認する。
    const auto existing = io_texture_indices.find(i_texture);
    if (existing != io_texture_indices.end())
    {
        o_index = existing->second;
        return true;
    }

    // 画像ファイルベースのテクスチャかどうか確認する。
    if (i_texture->type != UFBX_TEXTURE_FILE)
    {
        o_error = "画像ファイル以外のテクスチャ形式には対応していません: " +
            general::util::ToString(i_texture->name.data, i_texture->name.length);
        return false;
    }

    // テクスチャデータを初期化する。
    TextureData textureData;
    textureData.name = general::util::ToString(i_texture->name.data, i_texture->name.length);
    // FBX 内に画像データが埋め込まれていれば、それを優先して外部ファイルへの依存を避ける。
    if (i_texture->content.data != nullptr && i_texture->content.size > 0)
    {
        // 埋め込み画像のバイト列を取得する。
        const auto* bytes = static_cast<const std::uint8_t*>(i_texture->content.data);
        if (i_texture->content.size > textureData.rgba8.max_size())
        {
            o_error = "埋め込み画像のサイズが上限を超えています: " + textureData.name;
            return false;
        }

        // 埋め込み画像のバイト列をコピーする。
        auto imageBytes = std::vector<std::uint8_t>(bytes, bytes + i_texture->content.size);
        // 埋め込み画像のバイト列を文字列データにする。
        auto sourceName = general::util::ToString(i_texture->relative_filename.data, i_texture->relative_filename.length);
        if (sourceName.empty()){ sourceName = textureData.name; }   // ファイル名が空の場合はテクスチャ名を使用する。

        // 埋め込み画像をデコードする。
        if (!DecodeImage(imageBytes, sourceName, textureData, o_error))
        {
            return false;
        }
    }
    else
    {
        // 外部ファイルからテクスチャを読み込む。
        if (!LoadTextureFile(
                *i_texture,         // fbx読み込むテクスチャ情報。
                i_model_directory,  // FBX ファイルがあるディレクトリ。
                textureData,        // 読み込んだテクスチャデータを格納する。
                o_error))
        {
            return false;
        }
    }

    // テクスチャ枚数が上限に達していないか確認する。
    if (io_model.textures.size() >= std::numeric_limits<std::uint32_t>::max())
    {
        o_error = "テクスチャ数が上限を超えています。";
        return false;
    }
    o_index = static_cast<std::uint32_t>(io_model.textures.size());
    io_model.textures.push_back(std::move(textureData));
    io_texture_indices.emplace(i_texture, o_index);
    return true;
}

/**
 * @brief ufbx のマテリアル情報を描画用データへ変換する。
 * @param i_source FBX マテリアル。
 * @param i_model_directory FBX ファイルがあるディレクトリ。
 * @param io_model 読み込み中のモデル。
 * @param io_texture_indices テクスチャ対応表。
 * @param o_material 変換後のマテリアル。
 * @param o_error 変換失敗時の説明。
 * @return 変換に成功した場合は true。
 */
bool ConvertMaterial(
    const ufbx_material& i_source,
    const std::filesystem::path& i_model_directory,
    ModelData& io_model,
    std::unordered_map<const ufbx_texture*, std::uint32_t>& io_texture_indices,
    MaterialData& o_material,
    std::string& o_error)
{
    // マテリアル名を UTF-8 文字列として描画用データへ引き継ぐ。
    o_material.name = general::util::ToString(i_source.name.data, i_source.name.length);

    // PBR 値が存在しない場合は、FBX 形式固有の値を代替として使う。
    const auto* baseColor = &i_source.pbr.base_color;
    if (!baseColor->has_value)
    {
        baseColor = &i_source.fbx.diffuse_color;
    }
    if (baseColor->has_value)
    {
        // RGB を変換し、アルファ成分がない形式では完全不透明として扱う。
        o_material.base_color[0] = static_cast<float>(baseColor->value_vec4.x);
        o_material.base_color[1] = static_cast<float>(baseColor->value_vec4.y);
        o_material.base_color[2] = static_cast<float>(baseColor->value_vec4.z);
        o_material.base_color[3] = baseColor->value_components >= 4
            ? static_cast<float>(baseColor->value_vec4.w)
            : 1.0F;
    }

    // PBR の不透明度を優先し、未設定の場合は FBX の透明度を不透明度へ反転して変換する。
    if (i_source.pbr.opacity.has_value)
    {
        o_material.opacity = static_cast<float>(i_source.pbr.opacity.value_real);
    }
    else if (i_source.fbx.transparency_factor.has_value)    // FBX 形式固有の透明度を使用する。
    {
        o_material.opacity =
            1.0F - static_cast<float>(i_source.fbx.transparency_factor.value_real);
    }

    // PBR に粗さが含まれている場合だけ、描画用マテリアルへ反映する。
    if (i_source.pbr.roughness.has_value)
    {
        o_material.roughness = static_cast<float>(i_source.pbr.roughness.value_real);
    }

    // PBR に金属度が含まれている場合だけ、描画用マテリアルへ反映する。
    if (i_source.pbr.metalness.has_value)
    {
        o_material.metallic = static_cast<float>(i_source.pbr.metalness.value_real);
    }

    // PBR のベースカラー用テクスチャを優先し、未設定または無効なら FBX の拡散色テクスチャを使う。
    const auto* baseColorTexture = &i_source.pbr.base_color;
    if (baseColorTexture->texture == nullptr || !baseColorTexture->texture_enabled)
    {
        baseColorTexture = &i_source.fbx.diffuse_color;
    }

    // 有効なテクスチャだけをモデルに登録し、テクスチャ番号と UV セット名を記録する。
    if (baseColorTexture->texture != nullptr && baseColorTexture->texture_enabled)
    {
        std::uint32_t textureIndex = 0;
        if (!GetOrAddTexture(
                baseColorTexture->texture,
                i_model_directory,
                io_model,
                io_texture_indices,
                textureIndex,
                o_error))
        {
            return false;
        }
        o_material.base_color_texture   =   textureIndex;
        o_material.base_color_uv_set    =   general::util::ToString(
            baseColorTexture->texture->uv_set.data,
            baseColorTexture->texture->uv_set.length);
    }

    // 法線マップも PBR を優先し、使えない場合は FBX 形式固有の設定を参照する。
    const auto* normalTexture = &i_source.pbr.normal_map;
    if (normalTexture->texture == nullptr || !normalTexture->texture_enabled)
    {
        normalTexture = &i_source.fbx.normal_map;
    }
    if (normalTexture->texture != nullptr && normalTexture->texture_enabled)
    {
        // 読み込みまたは参照取得に失敗した場合は、エラーを設定して呼び出し元へ返す。
        std::uint32_t textureIndex = 0;
        if (!GetOrAddTexture(
                normalTexture->texture,
                i_model_directory,
                io_model,
                io_texture_indices,
                textureIndex,
                o_error))
        {
            return false;
        }
        o_material.normal_texture = textureIndex;
        o_material.normal_uv_set = general::util::ToString(
            normalTexture->texture->uv_set.data,
            normalTexture->texture->uv_set.length);
    }
    return true;
}

/**
 * @brief ufbx の 4x3 変換行列を列優先 4x4 行列へ変換する。
 * @param i_source ufbx の変換行列。
 * @return 描画側で使用する列優先 4x4 行列。
 */
std::array<float, 16> ConvertTransform(const ufbx_matrix& i_source)
{
    // v[0]～v[8] を回転・拡縮、v[9]～v[11] を平行移動に配置し、同次座標の行 [0, 0, 0, 1] を補う。
    // 列優先で格納するため、行列を行ごとに読むと [v0, v3, v6, v9]、[v1, v4, v7, v10]、
    // [v2, v5, v8, v11]、[0, 0, 0, 1] となる。
    return
    {
        static_cast<float>(i_source.v[0]), static_cast<float>(i_source.v[1]),
        static_cast<float>(i_source.v[2]), 0.0F,
        static_cast<float>(i_source.v[3]), static_cast<float>(i_source.v[4]),
        static_cast<float>(i_source.v[5]), 0.0F,
        static_cast<float>(i_source.v[6]), static_cast<float>(i_source.v[7]),
        static_cast<float>(i_source.v[8]), 0.0F,
        static_cast<float>(i_source.v[9]), static_cast<float>(i_source.v[10]),
        static_cast<float>(i_source.v[11]), 1.0F
    };
}

/**
 * @brief メッシュを三角形化し、頂点属性と面ごとのマテリアル番号を変換する。
 * @param i_node メッシュを持つノード。
 * @param i_material_indices FBX マテリアルとモデル内番号の対応表。
 * @param o_mesh 変換後のメッシュ。
 * @param o_error 変換失敗時の説明。
 * @return 変換に成功した場合は true。
 */
bool ConvertMesh(
    const ufbx_node& i_node,
    const std::unordered_map<const ufbx_material*, std::uint32_t>& i_material_indices,
    MeshData& o_mesh,
    std::string& o_error)
{
    const auto& source = *i_node.mesh;

    // 頂点情報やメッシュのインデックスに不整合がないか確認する。
    if (!source.vertex_position.exists ||
        source.vertex_position.indices.count < source.num_indices)
    {
        o_error = "メッシュに有効な頂点位置がありません: " +
            general::util::ToString(i_node.name.data, i_node.name.length);
        return false;
    }
    if (source.num_indices > std::numeric_limits<std::uint32_t>::max())
    {
        o_error = "メッシュの頂点インデックス数が上限を超えています: " +
            general::util::ToString(i_node.name.data, i_node.name.length);
        return false;
    }
    if (source.vertex_normal.exists &&
        source.vertex_normal.indices.count < source.num_indices)
    {
        o_error = "メッシュの法線インデックスが不正です: " +
            general::util::ToString(i_node.name.data, i_node.name.length);
        return false;
    }

    // メッシュ名とノード変換を引き継ぎ、頂点インデックス数に合わせて出力配列を用意する。
    o_mesh.name             =   general::util::ToString(i_node.name.data, i_node.name.length);  // メッシュ名
    o_mesh.node_transform   =   ConvertTransform(i_node.geometry_to_world);                     // ノードの姿勢変換行列
    // 角要素にあわせてメモリ領域だけを確保する。
    o_mesh.positions.resize(source.num_indices);
    o_mesh.normals.resize(source.num_indices);
    o_mesh.uv_sets.resize(source.uv_sets.count);
    o_mesh.uv_set_names.reserve(source.uv_sets.count);

    // UV セットごとに配列を確保し、取得に必要なインデックス数があることも確認する。
    for (std::size_t uvSetIndex = 0; uvSetIndex < source.uv_sets.count; ++uvSetIndex)
    {
        // UV セットの存在と頂点インデックスと対応しているか確認する。
        const auto& uv = source.uv_sets.data[uvSetIndex].vertex_uv;
        if (uv.exists && uv.indices.count < source.num_indices)
        {
            o_error = "メッシュの UV インデックスが不正です: " +
                general::util::ToString(i_node.name.data, i_node.name.length);
            return false;
        }

        // UV セット用の配列を確保する。
        o_mesh.uv_sets[uvSetIndex].resize(source.num_indices);
        o_mesh.uv_set_names.push_back(general::util::ToString(
            source.uv_sets.data[uvSetIndex].name.data,
            source.uv_sets.data[uvSetIndex].name.length));
    }

    // ufbx の取得関数で位置・法線・各 UV セットをインデックス単位に展開する。
    for (std::size_t vertexIndex = 0; vertexIndex < source.num_indices; ++vertexIndex)
    {
        // 頂点インデックスに対応する位置を取得し、出力配列に格納する。
        const auto position = ufbx_get_vertex_vec3(&source.vertex_position, vertexIndex);
        o_mesh.positions[vertexIndex] =
        {
            static_cast<float>(position.x),
            static_cast<float>(position.y),
            static_cast<float>(position.z)
        };

        // 法線が存在する場合は頂点インデックスに対応する法線を取得し、出力配列に格納する。
        if (source.vertex_normal.exists)
        {
            const auto normal = ufbx_get_vertex_vec3(&source.vertex_normal, vertexIndex);
            o_mesh.normals[vertexIndex] =
            {
                static_cast<float>(normal.x),
                static_cast<float>(normal.y),
                static_cast<float>(normal.z)
            };
        }

        // 各 UV セットについて頂点インデックスに対応する UV を取得し、出力配列に格納する。
        for (std::size_t uvSetIndex = 0; uvSetIndex < source.uv_sets.count; ++uvSetIndex)
        {
            const auto& uv = source.uv_sets.data[uvSetIndex].vertex_uv;
            if (uv.exists)
            {
                const auto value = ufbx_get_vertex_vec2(&uv, vertexIndex);
                o_mesh.uv_sets[uvSetIndex][vertexIndex] =
                {
                    static_cast<float>(value.x),
                    static_cast<float>(value.y)
                };
            }
        }
    }

    // 多角形の各面を三角形に分割し、生成した三角形へ元の面のマテリアルを割り当てる。
    for (std::size_t faceIndex = 0; faceIndex < source.faces.count; ++faceIndex)
    {
        const auto face = source.faces.data[faceIndex];
        // 3 頂点に満たない面からは三角形を作れないため読み飛ばす。
        if (face.num_indices < 3) { continue; }
        const std::size_t numTriangles = face.num_indices - 2;
        if (numTriangles > std::numeric_limits<std::size_t>::max() / 3)
        {
            o_error = "メッシュの面サイズが上限を超えています: " + o_mesh.name;
            return false;
        }

        // 多角形から作れる最大数で出力領域を確保してから、ufbx に三角形化を依頼する。
        auto triangleIndices = std::vector<std::uint32_t>(numTriangles * 3);
        const auto triangleCount = ufbx_triangulate_face(
            triangleIndices.data(),
            triangleIndices.size(),
            &source,
            face);
        const auto triangulatedCount    =   static_cast<std::size_t>(triangleCount) * 3;
        if (triangleCount == 0)
        {
            o_error = "メッシュの三角形化に失敗しました: " + o_mesh.name;
            return false;
        }
        if (triangulatedCount > triangleIndices.size())
        {
            o_error = "メッシュの三角形化に失敗しました: " + o_mesh.name;
            return false;
        }

        // メッシュインデックスの末尾に三角形インデックスを追加する。
        o_mesh.indices.insert(
            o_mesh.indices.end(),
            triangleIndices.begin(),
            triangleIndices.begin() + static_cast<std::ptrdiff_t>(triangulatedCount));

        // ノードにマテリアル割当があれば優先し、なければメッシュ側の割当を使う。
        std::uint32_t   modelMaterialIndex  =   0;
        const auto&     nodeMaterials       =   i_node.materials.count > 0 ? i_node.materials : source.materials;
        if (faceIndex < source.face_material.count)
        {
            // 面に割り当てられたマテリアルインデックスを取得する。
            const auto sourceMaterialIndex = source.face_material.data[faceIndex];
            if (sourceMaterialIndex != UFBX_NO_INDEX &&
                sourceMaterialIndex < nodeMaterials.count)
            {
                const auto material = i_material_indices.find(
                    nodeMaterials.data[sourceMaterialIndex]);
                if (material == i_material_indices.end())
                {
                    o_error = "メッシュが参照するマテリアルを特定できません: " + o_mesh.name;
                    return false;
                }
                modelMaterialIndex = material->second;
            }
        }
        else if (nodeMaterials.count == 1)  // ノードにマテリアルが1つだけ割り当てられている場合はそれを使用する。
        {
            const auto material = i_material_indices.find(nodeMaterials.data[0]);
            if (material == i_material_indices.end())
            {
                o_error = "メッシュが参照するマテリアルを特定できません: " + o_mesh.name;
                return false;
            }
            modelMaterialIndex = material->second;
        }

        // 同じ面から生成したすべての三角形に、選択したマテリアル番号を記録する。
        for (std::size_t triangleIndex = 0;
             triangleIndex < triangulatedCount / 3;
             ++triangleIndex)
        {
            o_mesh.triangle_material_indices.push_back(modelMaterialIndex);
        }
    }

    // 三角形が一つも生成されなければ、描画できないメッシュとして失敗を返す。
    if (o_mesh.indices.empty())
    {
        o_error = "描画可能な三角形がありません: " + o_mesh.name;
        return false;
    }
    return true;
}

/**
 * @brief FBX ファイルを解析して CPU 側モデルデータを作成する。
 * @param i_path 読み込む FBX ファイルの UTF-8 パス。
 * @param o_model 解析したモデルデータ。
 * @param o_error 読み込み失敗時の説明。
 * @return 解析に成功した場合は true。
 */
bool LoadModel(
    const std::string& i_path,
    ModelData& o_model,
    std::string& o_error)
{
    // 空のパスではファイルを特定できないため、読み込みを始める前に拒否する。
    if (i_path.empty())
    {
        o_error = "FBX ファイルのパスが空です。";
        return false;
    }

    // 呼び出し元の UTF-8 パスをファイルシステム用に変換して FBX の内容を読み込む。
    const auto fbxPath = general::util::PathFromUtf8(i_path);
    std::vector<std::uint8_t> fbxBytes;
    if (!general::util::ReadFile(fbxPath, fbxBytes, o_error))
    {
        return false;
    }

    // 外部テクスチャの相対パス解決に使うため、FBX の絶対パスと配置ディレクトリを求める。
    std::error_code fileSystemError;
    const auto absolutePath =
        std::filesystem::absolute(fbxPath, fileSystemError).lexically_normal();
    if (fileSystemError)
    {
        o_error = "FBX ファイルの絶対パスを解決できません: " + i_path;
        return false;
    }
    const auto modelDirectory = absolutePath.parent_path();
    const auto absolutePathString = general::util::PathToUtf8(absolutePath);

    // 静的モデルに必要な設定を指定し、アニメーションは読み込まず、不足法線は生成する。
    ufbx_load_opts loadOptions = {};
    loadOptions.ignore_animation            =   true;                                           // アニメーションの読み込みを無効にする。
    loadOptions.generate_missing_normals    =   true;                                           // 法線がない場合に生成する。
    loadOptions.index_error_handling        =   UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING;        // 不正なインデックスがあれば読み込みを中止する。
    loadOptions.use_blender_pbr_material    =   true;                                           // Blender 互換の PBR マテリアル解釈を使う。
    loadOptions.node_depth_limit            =   256;                                            // ノード階層の最大深度を 256 に制限する。
    loadOptions.filename.data               =   absolutePathString.data();                      // ufbx に渡す FBX 絶対パスの文字列データを設定する。
    loadOptions.filename.length             =   absolutePathString.size();                      // パス文字列のバイト数を設定する。
    loadOptions.target_unit_meters          =   1.0;                                            // モデルの単位をメートルに統一する。

    // シーンの寿命はこの関数内に限定し、モデルに必要な値は後続処理でコピーする。
    ufbx_error loadError = {};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
        ufbx_load_memory(fbxBytes.data(), fbxBytes.size(), &loadOptions, &loadError),
        ufbx_free_scene);
    if (!scene)
    {
        // ufbx の説明が得られていれば、読み込み元のパスと併せてエラーに含める。
        o_error = "FBX を解析できません: " + i_path;
        const auto detail = general::util::ToString(
            loadError.description.data,
            loadError.description.length);
        if (!detail.empty())
        {
            o_error += "。詳細: " + detail;
        }
        return false;
    }

    // シーン内のマテリアルを 32 ビット番号で参照できる範囲に制限する。
    if (scene->materials.count >= std::numeric_limits<std::uint32_t>::max())
    {
        o_error = "マテリアル数が上限を超えています: " + i_path;
        return false;
    }

    o_model = {};
    // マテリアル未指定の面が参照する既定番号として、空のマテリアルを先頭に置く。
    o_model.materials.emplace_back();
    // ufbx のポインターから、変換後モデル内のマテリアル・テクスチャ番号を引けるようにする。
    std::unordered_map<const ufbx_material*, std::uint32_t> materialIndices;
    std::unordered_map<const ufbx_texture*, std::uint32_t>  textureIndices;
    // シーンのマテリアルを順に変換し、後で面から参照できるよう対応表にも登録する。
    for (std::size_t materialIndex = 0;
         materialIndex < scene->materials.count;
         ++materialIndex)
    {
        // 描画用マテリアルへ変換する。
        MaterialData materialData;
        if (!ConvertMaterial(
                *scene->materials.data[materialIndex],
                modelDirectory,
                o_model,
                textureIndices,
                materialData,
                o_error))
        {
            return false;
        }

        // // 追加先のマテリアル番号を求め、ufbx マテリアルとの対応を登録してから配列へ追加する。
        const auto modelMaterialIndex   =   static_cast<std::uint32_t>(o_model.materials.size());
        materialIndices.emplace(scene->materials.data[materialIndex], modelMaterialIndex);
        o_model.materials.push_back(std::move(materialData));
    }

    // シーン内のメッシュを持つノードだけを変換し、モデルのメッシュ配列へ追加する。
    for (std::size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
    {
        // メッシュを持たないノードはスキップする。
        const auto& node = *scene->nodes.data[nodeIndex];
        if (node.mesh == nullptr) { continue; }

        // メッシュを持つノードを描画用メッシュへ変換する。
        MeshData meshData;
        if (!ConvertMesh(node, materialIndices, meshData, o_error))
        {
            return false;
        }
        o_model.meshes.push_back(std::move(meshData));
    }

    // マテリアルだけのシーンなど、描画できるメッシュがない入力は成功扱いにしない。
    if (o_model.meshes.empty())
    {
        o_error = "FBX に読み込み可能なメッシュがありません: " + i_path;
        return false;
    }
    return true;
}

} // namespace loader::fbx

namespace loader
{

/**
 * @brief 読み込んだ静的モデルデータを参照する。
 * @return 直近の読み込みに成功したモデルデータ。
 */
const ModelData& Loader<LoaderType::FBX>::GetModelData() const noexcept
{
    return m_instModelData;
}

/**
 * @brief FBX を解析し、静的モデルデータを保持する。
 * @param i_path 読み込む FBX ファイルの UTF-8 パス。
 * @return 読み込みに成功した場合は true。
 */
bool Loader<LoaderType::FBX>::LoadImpl(const std::string& i_path)
{
    m_instModelData = {};

    // FBX ファイルを解析してモデルデータを取得する。
    ModelData modelData;
    std::string error;
    if (!fbx::LoadModel(i_path, modelData, error))
    {
        SetLastError(error);
        return false;
    }

    // 解析に成功した場合はモデルデータを保持する。
    m_instModelData = std::move(modelData);
    return true;
}

} // namespace loader
