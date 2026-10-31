/**
 * @file wind_simulation.cpp
 * @brief 管理空間の追従と固定周期の風更新を調整する。
 * @date 2026-10-30
 * @par 最終更新日 2026-10-30
 */
#include "wind_simulation.h"
#include "wind_grid.h"
#include "wind_readback.h"
#include "wind_common.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace demo::wind::simulation
{
using detail::ValidateOrigin;
using detail::ValidateWind;

/**
 * @brief 空間追従、更新待ち時間と非同期読み戻しを所有する。
 */
struct WindSimulation::Impl
{
    WindRegion                              region;                               // 追従状態。
    WindGrid                                grid;                                 // GPU の風格子。
    std::array<int, windDefs::kAxisCount>    size;                                // 管理空間の寸法。
    std::array<float, windDefs::kAxisCount>  baseWind;                            // 現在の基本風。
    detail::WindReadback                    readback;                             // 非同期転送と取得済みの風。
    double                                  period                  = 1.0 / 60.0; // 固定更新の周期。
    double                                  pending                 = 0.0;        // 未処理の経過時間。
    std::uint64_t                           lastFrame               = 0;          // 受付済みの描画フレーム。
    double                                  cpuUpdateMilliseconds   = 0.0;        // 最後の CPU 更新時間。
    bool                                    hasCpuUpdate            = false;      // CPU 更新時間を取得済みか。
    bool                                    motorCacheRebuilt       = false;      // 最後の更新で発生器の寄与を再生成したか。

    /**
     * @brief コンストラクタ。
     * @param i_settings 管理空間の設定。
     * @param i_position 基準点の初期位置。
     * @param i_forward 初期視線。
     * @param i_baseWind 初期基本風。
     * @throws std::invalid_argument 設定、位置、視線または基本風が不正な場合。
     * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
     */
    Impl(const WindRegionSettings&                      i_settings, 
         const std::array<float, windDefs::kAxisCount>& i_position,
         const std::array<float, windDefs::kAxisCount>& i_forward,
         const std::array<float, windDefs::kAxisCount>& i_baseWind)
        : region(i_settings, i_position, i_forward)
        , grid(i_settings.size, i_baseWind)
        , size(i_settings.size)
        , baseWind(i_baseWind)
        , readback(size, region.Origin())
    {
    }
};

/**
 * @brief 初期位置、視線と基本風から管理空間と GPU 資源を作る。
 * @param i_settings 管理空間の寸法と追従条件。
 * @param i_position 基準点の初期位置。
 * @param i_forward 初期視線。
 * @param i_baseWind 初期基本風（m/s）。
 * @throws std::invalid_argument 設定、位置、視線または基本風が不正な場合。
 * @throws std::overflow_error 読み戻しサイズなどが表現範囲を超えた場合。
 * @throws std::runtime_error GPU 資源やシェーダーを作成できない場合。
 * @pre 有効な OpenGL 4.3 コンテキストを持つスレッドで呼び出す。
 */
WindSimulation::WindSimulation(const WindRegionSettings&                        i_settings,
                               const std::array<float, windDefs::kAxisCount>&   i_position,
                               const std::array<float, windDefs::kAxisCount>&   i_forward,
                               const std::array<float, windDefs::kAxisCount>&   i_baseWind)
    : m_instImpl(std::make_unique<Impl>(i_settings, i_position, i_forward, i_baseWind))
{
}

/**
 * @brief 風格子と非同期読み戻しに使う GPU 資源を解放する。
 * @pre 作成時と同じ有効な OpenGL コンテキストを持つスレッドで呼び出す。
 */
WindSimulation::~WindSimulation() = default;

/**
 * @brief 固定周期の風更新を 1 秒あたり何回行うか設定する。
 * @param i_hertz 更新回数。周期を正の有限値として表現できる値。
 * @throws std::invalid_argument 更新回数または周期が不正な場合。
 * @details 周期の変更後も未処理の経過時間は保持する。
 */
void WindSimulation::SetUpdateRate(const double i_hertz)
{
    const auto period   =   1.0 / i_hertz;
    const auto step     =   static_cast<float>(period);
    if (!std::isfinite(i_hertz) ||
        i_hertz <= 0.0 ||
        !std::isfinite(period) ||
        !std::isfinite(step) ||
        step <= 0.0F)
    {
        throw std::invalid_argument("風の更新回数には周期を表現できる正の有限値を指定してください。");
    }
    m_instImpl->period = period;
}

/**
 * @brief 60 Hz で 1 回更新する場合の風の分配割合を設定する。
 * @param i_fraction 0 以上 1 以下の有限値。
 * @throws std::invalid_argument 割合が範囲外または非有限の場合。
 * @details 風速比例方式では 64 m/s のときの割合を指定する。
 */
void WindSimulation::SetDistribution(const float i_fraction)
{
    m_instImpl->grid.SetDistribution(i_fraction);
}

/**
 * @brief 風速によらない方式と風速に比例する方式を切り替える。
 * @param i_method 使用する分配方式。
 * @throws std::invalid_argument 方式が不正な場合。
 */
void WindSimulation::SetDistributionMethod(const WindDistribution i_method)
{
    m_instImpl->grid.SetDistributionMethod(i_method);
}

/**
 * @brief 目標風との差が半分になる時間を設定する。
 * @param i_seconds 半減時間（秒）。正の有限値。
 * @throws std::invalid_argument 時間が不正な場合。
 */
void WindSimulation::SetHalfLife(const float i_seconds)
{
    m_instImpl->grid.SetHalfLife(i_seconds);
}

/**
 * @brief 管理空間の追従、固定周期の風更新と非同期読み戻しを進める。
 * @param i_elapsed 前回からの経過時間（秒）。0 以上の有限値。
 * @param i_frame 1 以上の描画フレーム番号。前回正常終了した番号より大きい値。
 * @param i_position 追従する基準点の位置。
 * @param i_forward 基準点の視線。
 * @param i_baseWind 現在の基本風（m/s）。
 * @param io_motors 反映する風発生器。更新成功後に瞬間発生器を解放する。
 * @param i_occupancy セル内の占有率を近似する精度。
 * @return 実行した風の更新回数。
 * @throws std::invalid_argument 経過時間、フレーム番号、位置、視線、基本風または精度が不正な場合。
 * @throws std::overflow_error 未処理時間、更新回数または管理空間が表現範囲を超えた場合。
 * @throws std::runtime_error GPU 処理または読み戻しに失敗した場合。
 * @details 転送枠が全て使用中の場合は新しい転送を見送り、取得済みデータを保持する。
 * 入力と追従先の検証後に失敗した場合は成功済みの更新と未処理時間を保持するため、
 * 経過時間を二重計上しないよう再試行では 0 秒を渡す。
 * プログラム、画像ユニット 0～5、シェーダーストレージの結合点 0～1 を変更する。
 * @pre 作成時と同じ有効な OpenGL コンテキストを持つスレッドで呼び出す。
 */
std::uint64_t WindSimulation::Advance(const double                                      i_elapsed,
                                      const std::uint64_t                               i_frame,
                                      const std::array<float, windDefs::kAxisCount>&    i_position,
                                      const std::array<float, windDefs::kAxisCount>&    i_forward,
                                      const std::array<float, windDefs::kAxisCount>&    i_baseWind,
                                      motor::WindMotors&                                io_motors,
                                      const OccupancySettings&                          i_occupancy)
{
    auto& impl          =   *m_instImpl;

    // CPU処理計測開始
    const auto cpuStart =   std::chrono::steady_clock::now();

    // 設定の検証
    if (!std::isfinite(i_elapsed) ||
        i_elapsed < 0.0 ||
        i_frame == 0 || 
        i_frame <= impl.lastFrame ||
        i_occupancy.subdivisions < defs::kMinimumSubdivisions ||
        i_occupancy.subdivisions > defs::kMaximumSubdivisions)
    {
        throw std::invalid_argument("経過時間、フレーム番号または占有率設定が不正です。");
    }

    // 基本風の検証
    ValidateWind(i_baseWind);
    const auto total    =   impl.pending + i_elapsed;   // 未処理時間の合計
    if (!std::isfinite(total) ||
        (i_elapsed > 0.0 && total == impl.pending) ||
        (total >= impl.period && total - impl.period == total) ||
        total / impl.period >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
    {
        throw std::overflow_error("風の未処理時間が表現範囲を超えています。");
    }

    // 移動先をコピーで決めて検証し、移動に失敗しても現在の追従状態を保持する。
    auto        nextRegion  =   impl.region;
    const auto  move        =   nextRegion.Update(i_position, i_forward);
    ValidateOrigin(move.origin, impl.size);

    // 検証後は経過時間を保持する。以降の処理に失敗した場合、再試行時は 0 秒を渡す。
    impl.pending = total;

    // 管理空間を移動する前に、完了済みの読み戻しを待機せず回収する。
    impl.readback.Poll();

    // 移動先が変わった場合は重複領域の風を保持して格子を移動し、成功後に追従状態を確定する。
    if (move.updated) { impl.grid.Move(move.offset, i_baseWind); }
    impl.region = nextRegion;

    // 移動がない場合も、範囲外の代替風などで使う基本風を更新する。
    impl.baseWind = i_baseWind;

    // 経過時間を捨てず、正常に実行できた更新の時間だけを未処理時間から差し引く。
    auto count          =   std::uint64_t{};
    auto motorRebuilt   =   false;
    while (impl.pending >= impl.period)
    {
        if (count == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("風の更新回数が表現範囲を超えています。");
        }

        // 風を一周期分更新し、今回の Advance 中に発生器の寄与を再計算したかを累積する。
        impl.grid.Step(static_cast<float>(impl.period), i_baseWind, impl.region.Origin(), io_motors, i_occupancy);
        motorRebuilt = motorRebuilt || impl.grid.Performance().motorCacheRebuilt;

        // 更新に成功した分だけ未処理時間を減らし、実行回数を増やす。
        impl.pending -= impl.period;
        ++count;
    }

    // 空き枠があれば現在の風を原点・フレーム番号とともに非同期で読み戻し、処理したフレームを記録する。
    impl.readback.Submit(impl.grid.Textures(), impl.region.Origin(), i_frame);
    impl.lastFrame = i_frame;

    // 今回の経過時間と、発生器の寄与を再計算したかを記録する。
    impl.cpuUpdateMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpuStart).count();
    impl.hasCpuUpdate       =   true;
    impl.motorCacheRebuilt  =   motorRebuilt;

    // 風の更新回数
    return count;
}

/**
 * @brief CPU 更新時間、取得済みの GPU 時間と発生器キャッシュの状態を取得する。
 * @return 最後に正常終了した更新の CPU 時間と、最後に取得した GPU 計測値。
 * @details GPU 計測値は非同期に取得されるため、現在のフレームと一致するとは限らない。
 */
WindPerformanceStats WindSimulation::Performance() const
{
    auto result                     =   m_instImpl->grid.Performance();       // 格子側の GPU 計測値を取得する。
    result.cpuUpdateMilliseconds    =   m_instImpl->cpuUpdateMilliseconds;    // 最後に正常終了した更新の経過時間。
    result.hasCpuUpdate             =   m_instImpl->hasCpuUpdate;             // CPU 更新時間を取得済みか。
    result.motorCacheRebuilt        =   m_instImpl->motorCacheRebuilt;        // 最後の更新中に発生器の寄与を再計算したか。
    return result;
}


/**
 * @brief 最後に完了した読み戻しからワールド座標の風を取得する。
 * @param i_position 検索するワールド座標。各成分は有限値。
 * @param i_fallback 現在または取得済みの管理空間の範囲外で返す代替風。
 * @return 風速、取得状態と読み戻し元のフレーム番号。
 * @throws std::invalid_argument 座標または代替風の指定が不正な場合。
 * @details 初回の読み戻しを待っている間は、指定した代替風にかかわらず基本風を返す。
 */
WindSample WindSimulation::Sample(const std::array<double, windDefs::kAxisCount>&   i_position,
                                  const WindFallback                                i_fallback) const
{
    return m_instImpl->readback.Sample(i_position, m_instImpl->region.Origin(), m_instImpl->baseWind, i_fallback);
}

/**
 * @brief 現在の管理空間の最小ワールド座標を取得する。
 * @return XYZ 各軸の整数セル座標。
 */
std::array<int, windDefs::kAxisCount> WindSimulation::Origin() const
{
    return m_instImpl->region.Origin();
}

/**
 * @brief 現在の GPU 風速テクスチャを取得する。
 * @return XYZ 成分順の R32I テクスチャ名。
 * @details 所有権は移らない。更新後はテクスチャ名を再取得する。
 */
std::array<unsigned int, windDefs::kAxisCount> WindSimulation::Textures() const
{
    return m_instImpl->grid.Textures();
}
}
