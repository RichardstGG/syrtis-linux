#include "QuotaTrend.h"

#include <algorithm>
#include <cmath>

namespace syrtis {
std::optional<QuotaTrend> QuotaTrendFold::trend(
    double usedPercent, qint64 windowStartMs, qint64 windowEndMs, qint64 nowMs,
    const std::vector<QuotaSample> &samples)
{
    const auto durationMs = windowEndMs - windowStartMs;
    if (durationMs <= 0 || nowMs <= windowStartMs)
        return std::nullopt;

    std::vector<QuotaSample> inside;
    for (const auto &sample : samples) {
        if (sample.atMs >= windowStartMs && sample.atMs <= nowMs)
            inside.push_back(sample);
    }
    std::stable_sort(inside.begin(), inside.end(), [](const auto &a, const auto &b) {
        return a.atMs < b.atMs;
    });
    if (inside.empty())
        return std::nullopt;

    // std::round, like Swift .rounded(), rounds a positive half away from zero.
    const auto spanStartMs = inside.back().atMs
        - qint64(std::round(double(durationMs) * lookbackFraction));
    const auto first = std::find_if(inside.begin(), inside.end(), [spanStartMs](const auto &sample) {
        return sample.atMs >= spanStartMs;
    });
    const auto &last = inside.back();
    if (size_t(inside.end() - first) < minimumSamples || last.atMs <= first->atMs)
        return std::nullopt;

    const double elapsedFraction = double(last.atMs - first->atMs) / double(durationMs);
    const double recentSlope = (last.usedPercent - first->usedPercent) / (elapsedFraction * 100);
    const double windowElapsedFraction = std::clamp(double(nowMs - windowStartMs) / double(durationMs), 0.0, 1.0);
    const double rawDelta = recentSlope * (1 - windowElapsedFraction) * 100;
    // Floor provider corrections, but preserve >100% as the run-out signal.
    const double projected = std::max(0.0, usedPercent + rawDelta);
    const auto direction = std::abs(recentSlope) <= flatThreshold
        ? QuotaTrend::Direction::flat
        : (recentSlope > 0 ? QuotaTrend::Direction::rising : QuotaTrend::Direction::falling);
    return QuotaTrend{direction, projected, projected - usedPercent};
}
}
