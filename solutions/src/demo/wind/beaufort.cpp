/**
 * @file beaufort.cpp
 * @brief ビューフォート風力階級を風速で分類する。
 * @date 2026-10-29
 * @par 最終更新日 2026-10-29
 */
#include "beaufort.h"
#include "defs/beaufort_constants.h"
#include <cmath>
#include <stdexcept>

namespace demo::wind
{

/**
 * @brief   風速からビューフォート風力階級を求める。
 * @param i_speedMetersPerSecond 負でない有限な風速（m/s）。
 * @return 対応する階級と日本語名称。
 * @throws std::invalid_argument 風速が負または有限でない場合。
 * @details 各階級の下限を境界とし、32.7 m/s 以上は風力 12 とする。
 */
BeaufortScale ClassifyBeaufort(const float i_speedMetersPerSecond)
{
    if (!std::isfinite(i_speedMetersPerSecond) || i_speedMetersPerSecond < 0.0F)
    {
        throw std::invalid_argument("風速は負でない有限値を指定してください。");
    }

    // ビューフォート風力階級を決定する
    auto level = std::size_t{};
    while (level < defs::kBeaufortThresholds.size() &&
           i_speedMetersPerSecond >= defs::kBeaufortThresholds[level])
    {
        ++level;
    }

    // ビューフォート風力階級の名称を取得する（風の強度の参考に）
    return {static_cast<int>(level), defs::kBeaufortNames[level]};
}

}
