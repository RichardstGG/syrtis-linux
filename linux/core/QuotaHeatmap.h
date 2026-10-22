#pragma once

#include <QtCore/QTimeZone>
#include <QtCore/QtGlobal>

#include <array>
#include <vector>

namespace syrtis {

// The three fields consumed from the producer's QuotaCurvePoint. Wire decoding
// belongs to the caller; this fold never reads the history store or calls FFI.
struct QuotaCurvePoint {
    qint64 sampledAt;
    double usedPercent;
    qint64 resetAt;
};

struct QuotaHeatmap {
    // Monday = 0, Sunday = 6; local hours 0...23.
    std::array<std::array<double, 24>, 7> cells{};
    double peak = 0;
    double total = 0;
    double unplacedPercent = 0;
    qsizetype observedDays = 0;

    bool isEmpty() const { return total <= 0; }
    bool hasMovement() const { return !isEmpty() || unplacedPercent > 0; }
    bool operator==(const QuotaHeatmap &) const = default;
};

namespace QuotaHeatmapFold {
inline constexpr qint64 maximumGapSeconds = 6 * 3600;

// Gregorian local calendar, matching the Swift fold's default calendar.
QuotaHeatmap build(const std::vector<QuotaCurvePoint> &points,
                   const QTimeZone &timeZone = QTimeZone::systemTimeZone());
}
}
