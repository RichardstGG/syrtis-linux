#pragma once
#include <QtCore/QtTypes>
#include <algorithm>
#include <optional>

namespace Syrtis {
// QuotaHistory.swift value dependency of QuotaOverview. History folding/joining
// remains a separate port; this value never reconstructs producer aggregates.
struct QuotaCycle {
    qint64 resetAtMs, startMs;
    double usedPercent;
    qint64 risingRuns;
    double peakUsedPercent;
    qint64 sampleCount, firstSampleMs, lastSampleMs;
    double observedFraction;
    qint64 evidenceStartMs() const { return std::min(startMs, firstSampleMs); }
    qint64 durationMs() const { return resetAtMs - startMs; }
    QuotaCycle(qint64 resetAtMs, qint64 startMs, double usedPercent,
        qint64 sampleCount, double observedFraction, qint64 firstSampleMs = 0,
        qint64 lastSampleMs = 0, std::optional<double> peakUsedPercent = std::nullopt,
        qint64 risingRuns = 1)
        : resetAtMs(resetAtMs), startMs(startMs), usedPercent(usedPercent), risingRuns(risingRuns),
          peakUsedPercent(peakUsedPercent.value_or(usedPercent)), sampleCount(sampleCount),
          firstSampleMs(firstSampleMs), lastSampleMs(lastSampleMs), observedFraction(observedFraction) {}
    bool operator==(const QuotaCycle &) const = default;
};
}
