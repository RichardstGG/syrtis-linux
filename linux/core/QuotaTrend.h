#pragma once

#include <QtCore/QtGlobal>

#include <optional>
#include <vector>

namespace syrtis {

// Display sample from WindowCardGeometry.swift; times are milliseconds.
struct QuotaSample {
    qint64 atMs;
    double usedPercent;
};

struct QuotaTrend {
    enum class Direction { rising, falling, flat };

    Direction direction;
    double projectedUsedPercent;
    double projectedDeltaPercent;

    bool runsOutEarly() const { return projectedUsedPercent > 100; }
    bool operator==(const QuotaTrend &) const = default;
};

namespace QuotaTrendFold {
inline constexpr double lookbackFraction = 0.25;
inline constexpr size_t minimumSamples = 2;
inline constexpr double flatThreshold = 0.05;

std::optional<QuotaTrend> trend(double usedPercent, qint64 windowStartMs,
                               qint64 windowEndMs, qint64 nowMs,
                               const std::vector<QuotaSample> &samples);
}
}
