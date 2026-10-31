/**
 * @file wind_readback.cpp
 * @brief GPU の風格子を非同期に読み戻し、取得済みの値を参照する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "wind_readback.h"
#include "wind_common.h"
#include "../defs/wind_region_constants.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace demo::wind::simulation::detail
{
    
namespace
{

/**
 * @brief 読み戻し時の転送設定を一時的に整え、呼び出し側の設定を復元する。
 */
struct PackState
{
    GLint texture{};                    // 現在の 3D テクスチャのバインディング
    GLint buffer{};                     // 現在のピクセルパックバッファのバインディング
    std::array<GLint, 7> settings{};    // 現在のピクセルパック設定の保存用
    static constexpr auto kPackSettings = std::array
    {
        GL_PACK_ALIGNMENT,       // 各行の開始位置をそろえるバイト境界
        GL_PACK_ROW_LENGTH,      // 1行あたりのピクセル数
        GL_PACK_IMAGE_HEIGHT,    // 1画像あたりの行数
        GL_PACK_SKIP_PIXELS,     // 書き込み先で読み飛ばすピクセル数
        GL_PACK_SKIP_ROWS,       // 書き込み先で読み飛ばす行数
        GL_PACK_SKIP_IMAGES,     // 書き込み先で読み飛ばす画像数
        GL_PACK_SWAP_BYTES       // 複数バイト要素のバイト順を入れ替えるか
    };

    /**
     *  @brief コンストラクタ。現在の転送設定を保存し、CPU の初期値を設定する。
     */
    PackState()
    {
        // バッファ取得
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
        for (auto index = std::size_t{}; index < kPackSettings.size(); ++index)
        {
            glGetIntegerv(kPackSettings[index], &settings[index]);
            glPixelStorei(kPackSettings[index], index == 0 ? 1 : 0);    // アライメントだけ 1 バイト設定
        }
    }

    /**
     *  @brief デストラクタ。保存していた転送設定を復元する。
     */
    ~PackState()
    {
        for (auto index = std::size_t{}; index < kPackSettings.size(); ++index)
        {
            glPixelStorei(kPackSettings[index], settings[index]);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(buffer));
        glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(texture));
    }
};

}

/**
 * @brief 非同期転送枠と最後に取得したデータを保持する。
 */
struct WindReadback::Impl
{
    /**
     * @brief 読み戻し中の GPU バッファーと、その内容に対応する管理空間。
     */
    struct Transfer
    {
        GLuint                                  buffer   =   0;         // 非同期転送先
        GLsync                                  fence    =   nullptr;   // 転送完了の確認用
        std::array<int, windDefs::kAxisCount>   origin{};               // 転送時の原点
        std::uint64_t                           frame    =   0;         // 転送元フレーム
    };

    std::array<int, windDefs::kAxisCount>   size;                   // 格子の寸法。
    std::array<Transfer, 3>                 transfers{};            // 転送枠。
    std::vector<std::int32_t>               samples;                // 最後に取得した風。
    std::array<int, windDefs::kAxisCount>   sampleOrigin{};         // 取得時の原点。
    std::uint64_t                           sampleFrame     = 0;    // 取得元のフレーム。
    std::size_t                             cells           = 0;    // 成分あたりのセル数。

    /**
     *  @brief コンストラクタ。
     *  @param i_size 格子の寸法。
     *  @param i_origin 原点。
     */
    Impl(const std::array<int, windDefs::kAxisCount>& i_size,
         const std::array<int, windDefs::kAxisCount>& i_origin) 
         : size(i_size)
    {
        ValidateOrigin(i_origin, size);

        // 転送可能な最大セル数を計算する
        const auto maximum = std::min(static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()),
                                      std::numeric_limits<std::size_t>::max()) / (windDefs::kAxisCount * sizeof(std::int32_t));
        cells = 1;
        for (const auto extent : size)
        {
            if (extent <= 0) { throw std::invalid_argument("風の読み戻し寸法は正にしてください。"); }
            if (cells > maximum / static_cast<std::size_t>(extent))
            {
                throw std::overflow_error("風の読み戻しサイズが範囲外です。");
            }
            cells *= static_cast<std::size_t>(extent);
        }
    }

    /**
     * @brief デストラクタ。
     */
    ~Impl()
    {
        for (auto& transfer : transfers)
        {
            if (transfer.fence != nullptr) { glDeleteSync(transfer.fence); }        // 完了確認用の同期オブジェクトを削除
            if (transfer.buffer != 0) { glDeleteBuffers(1, &transfer.buffer); }     // 非同期転送先のバッファーを削除
        }
    }

    /**
     * @brief 完了した転送だけを読み、未完了の場合は以前の結果を維持する。
     */
    void Poll()
    {
        while (true)
        {
            // 取得済みデータが古いフレームに戻らないよう、未回収の転送をフレーム番号順に処理する
            auto* oldest = static_cast<Transfer*>(nullptr);
            for (auto& transfer : transfers)
            {
                if (transfer.fence != nullptr && (oldest == nullptr || transfer.frame < oldest->frame))
                {
                    oldest = &transfer;
                }
            }
            if (oldest == nullptr) { return; }

            // 最も古い転送の完了を待機せず確認し、未完了なら次回の呼び出しに回す。
            const auto status = glClientWaitSync(oldest->fence, 0, 0);
            if (status == GL_TIMEOUT_EXPIRED) { return; }

            // 完了以外の結果は読み戻しに使わず、OpenGL エラーまたは確認失敗を通知する。
            if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
            {
                CheckGl("風の読み戻し完了確認");
                throw std::runtime_error("風の読み戻しの完了を確認できません。");
            }

            // GPU が書き終えたバッファーだけを写し、成功後に取得済み情報を更新する。
            auto        values  =   std::vector<std::int32_t>(cells * windDefs::kAxisCount);
            const auto  state   =   PackState{};
            glBindBuffer(GL_PIXEL_PACK_BUFFER, oldest->buffer);

            // 転送先を CPU から読み取れるようにし、領域を取得できなければ既存の取得結果を保持する。
            const auto bytes    =   static_cast<GLsizeiptr>(values.size() * sizeof(std::int32_t));
            const auto data     =   glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT);
            if (data == nullptr)
            {
                CheckGl("風の読み戻し");
                throw std::runtime_error("風の読み戻し領域を取得できません。");
            }

            // CPU 側へコピーしてからマップを解除し、破損していないことを確認する。
            std::memcpy(values.data(), data, static_cast<std::size_t>(bytes));
            const auto valid = glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            CheckGl("風の読み戻し");
            if (valid != GL_TRUE) { throw std::runtime_error("風の読み戻しデータが破損しました。"); }

            // コピーが成功した場合だけ風と転送元の原点・フレームを更新する。
            samples = std::move(values);
            sampleOrigin    =   oldest->origin;
            sampleFrame     =   oldest->frame;

            // 完了確認用フェンスを解放し、バッファーは次の転送で再利用する。
            glDeleteSync(oldest->fence);
            oldest->fence = nullptr;
        }
    }

    /**
     * @brief 空き枠がある場合に風格子の非同期読み戻しを開始する。
     * @param i_textures 格子の寸法に一致する XYZ 成分順の R32I 形式の 3D テクスチャ名。
     * @param i_origin 転送元の管理空間の原点。
     * @param i_frame 転送元の描画フレーム番号。1 以上で、前回の呼び出しより大きい値。
     * @throws std::runtime_error バッファーやフェンスの作成、転送の開始に失敗した場合。
     * @details 空き枠がない場合は転送を見送り、取得済みデータを保持する。
     * 完了したデータは後の Poll() で回収する。呼び出し元のピクセル転送設定と
     * 3D テクスチャの結合は復元する。
     * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
     */
    void Submit(const std::array<unsigned int, windDefs::kAxisCount>&   i_textures,
                const std::array<int, windDefs::kAxisCount>&            i_origin,
                const std::uint64_t                                     i_frame)
    {
        // 未回収の転送を上書きしないよう、フェンスのない枠だけを使う。
        auto* slot = static_cast<Transfer*>(nullptr);
        for (auto& transfer : transfers)
        {
            if (transfer.fence == nullptr) { slot = &transfer; break; }
        }
        if (slot == nullptr) { return; }

        // 呼び出し元の転送設定を保存し、必要なら読み戻し先のバッファーを作る。
        CheckGl("風の読み戻し開始前");
        const auto state = PackState{};
        if (slot->buffer == 0) { glGenBuffers(1, &slot->buffer); }
        if (slot->buffer == 0) { throw std::runtime_error("風の読み戻し用バッファーを作成できません。"); }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, slot->buffer);
        const auto planeBytes = cells * sizeof(std::int32_t);
        glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(planeBytes * windDefs::kAxisCount), nullptr,
                     GL_STREAM_READ);
        const auto& textures = i_textures;

        // XYZ 各成分をバッファー内の別々の位置へ転送する。
        for (auto axis = std::size_t{}; axis < textures.size(); ++axis)
        {
            glBindTexture(GL_TEXTURE_3D, textures[axis]);
            glGetTexImage(GL_TEXTURE_3D, 0, GL_RED_INTEGER, GL_INT,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(axis * planeBytes)));
        }

        // 転送要求に失敗したバッファーは再利用せず、原因を呼び出し側に通知する。
        const auto transferError = glGetError();
        if (transferError != GL_NO_ERROR)
        {
            glDeleteBuffers(1, &slot->buffer);
            slot->buffer = 0;
            throw std::runtime_error("風の読み戻し開始: OpenGL エラー " + std::to_string(transferError));
        }

        // 転送完了を後で確認できるようフェンスを置く。作成に失敗した場合はバッファーも解放する。
        slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (slot->fence == nullptr)
        {
            glDeleteBuffers(1, &slot->buffer);
            slot->buffer = 0;
            CheckGl("風の読み戻し開始");
            throw std::runtime_error("風の読み戻しの完了確認を作成できません。");
        }

        // 取得した風を後でワールド座標と対応付けられるよう、転送時の原点とフレームを保持する。
        slot->origin = i_origin;
        slot->frame = i_frame;

        // GPU に処理を送る。完了は待たず、送信に失敗した場合は転送枠の資源を解放する。
        glFlush();
        const auto flushError = glGetError();
        if (flushError != GL_NO_ERROR)
        {
            glDeleteSync(slot->fence);
            slot->fence = nullptr;
            glDeleteBuffers(1, &slot->buffer);
            slot->buffer = 0;
            throw std::runtime_error("風の読み戻し開始: OpenGL エラー " + std::to_string(flushError));
        }
    }
};

/**
 * @brief WindReadback のコンストラクタ。
 * @param i_size 格子の寸法。
 * @param i_origin 初期の管理空間の原点。
 * @throws std::invalid_argument サイズや原点が不正な場合。
 * @details 内部実装を初期化する。
 */
WindReadback::WindReadback(const std::array<int, windDefs::kAxisCount>& i_size,
                           const std::array<int, windDefs::kAxisCount>& i_origin)
    : m_instImpl(std::make_unique<Impl>(i_size, i_origin)) {}

/**
 * @brief WindReadback のデストラクタ。
 * @details 内部実装を破棄する。
 */
WindReadback::~WindReadback() = default;

/**
 * @brief 完了した転送だけを回収する。
 */
void WindReadback::Poll() { m_instImpl->Poll(); }

/**
 * @brief 空き枠がある場合に風格子の非同期読み戻しを開始する。
 * @param i_textures 格子の寸法に一致する XYZ 成分順の R32I 形式の 3D テクスチャ名。
 * @param i_origin 転送元の管理空間の原点。
 * @param i_frame 転送元の描画フレーム番号。1 以上で、前回の呼び出しより大きい値。
 * @throws std::runtime_error バッファーやフェンスの作成、転送の開始に失敗した場合。
 * @details 空き枠がない場合は転送を見送り、取得済みデータを保持する。
 * 完了したデータは後の Poll() で回収する。
 * @pre 作成時と同じ有効な OpenGL コンテキスト上で呼び出す。
 */
void WindReadback::Submit(const std::array<unsigned int, windDefs::kAxisCount>& i_textures,
                          const std::array<int, windDefs::kAxisCount>& i_origin,
                          const std::uint64_t i_frame)
{
    m_instImpl->Submit(i_textures, i_origin, i_frame);
}

/**
 * @brief 取得済みの風から指定したワールド座標の値を返す。
 * @param i_position 取得するワールド座標。各成分は有限値。
 * @param i_origin 現在の管理空間の原点。
 * @param i_baseWind 初回の読み戻し待ちや代替風として使う現在の基本風（m/s）。
 * @param i_fallback 管理空間または取得済み範囲の外で返す代替風。
 * @return 風速、取得状態と取得元のフレーム番号。
 * @throws std::invalid_argument 座標または代替風の指定が不正な場合。
 * @details 現在の管理空間内で初回の読み戻しを待つ間は、代替風の指定にかかわらず基本風を返す。
 */
WindSample WindReadback::Sample(const std::array<double, windDefs::kAxisCount>& i_position,
                                const std::array<int, windDefs::kAxisCount>& i_origin,
                                const std::array<float, windDefs::kAxisCount>& i_baseWind,
                                const WindFallback i_fallback) const
{
    // 非有限座標や未定義の代替風指定を、範囲判定や配列添字の計算に使わない。
    if (!std::all_of(
        i_position.begin(),
        i_position.end(),
        [](const auto i_value) { 
            return std::isfinite(i_value); 
        }) ||
        (i_fallback != WindFallback::BaseWind && i_fallback != WindFallback::Calm))
    {
        throw std::invalid_argument("風の取得座標または代替風の指定が不正です。");
    }

    // 範囲外では、指定に応じて現在の基本風または無風を返す。
    const auto& impl    = *m_instImpl;
    const auto fallback = i_fallback == WindFallback::BaseWind
        ? i_baseWind : std::array<float, windDefs::kAxisCount>{};

    // 最小座標を含み、最大座標を含まない範囲として各軸を判定する。
    const auto inside = [&i_position, &impl](const auto& i_origin)
    {
        for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
        {
            if (i_position[axis] < i_origin[axis] ||
                i_position[axis] >= static_cast<double>(i_origin[axis]) + impl.size[axis]) { return false; }
        }
        return true;
    };

    // 現在の空間外は代替風、初回読み戻し前の空間内は準備中として基本風を返す。
    if (!inside(i_origin)) { return {fallback, WindSampleStatus::Fallback, 0}; }
    if (impl.sampleFrame == 0) { return {i_baseWind, WindSampleStatus::Preparing, 0}; }

    // 空間移動後に新たに入った範囲は、読み戻しが完了するまで代替風を返す。
    if (!inside(impl.sampleOrigin)) { return {fallback, WindSampleStatus::Fallback, 0}; }

    // 取得時の原点を使い、空間移動後も同じワールド座標の値を参照する。
    const auto x = static_cast<std::size_t>(std::floor(i_position[windDefs::kAxisXIndex] - impl.sampleOrigin[windDefs::kAxisXIndex]));
    const auto y = static_cast<std::size_t>(std::floor(i_position[windDefs::kAxisYIndex] - impl.sampleOrigin[windDefs::kAxisYIndex]));
    const auto z = static_cast<std::size_t>(std::floor(i_position[windDefs::kAxisZIndex] - impl.sampleOrigin[windDefs::kAxisZIndex]));

    // X が最も速く変化する順序でセル位置を配列添字に変換する。
    const auto index = x + static_cast<std::size_t>(impl.size[windDefs::kAxisXIndex]) *
                                                    (y + static_cast<std::size_t>(impl.size[windDefs::kAxisYIndex]) * z);
    auto wind = std::array<float, windDefs::kAxisCount>{};

    // 成分別に保存された 16.16 固定小数点の値を風速に戻す。
    for (auto axis = std::size_t{}; axis < windDefs::kAxisCount; ++axis)
    {
        wind[axis] = static_cast<float>(impl.samples[axis * impl.cells + index]) / kScale;
    }
    return {wind, WindSampleStatus::Ready, impl.sampleFrame};
}

}   // namespace demo::wind::simulation::detail
