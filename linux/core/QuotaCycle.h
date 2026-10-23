#pragma once
#include <QtCore/QtTypes>
#include <algorithm>
#include <optional>

// QuotaHistory.swift's QuotaCycle, shared by QuotaHistory and QuotaOverview.
namespace syrtis::core {
struct QuotaCycle {
    qint64 resetAtMs = 0, startMs = 0;
    double usedPercent = 0;
    qsizetype sampleCount = 0;
    double observedFraction = 0;
    qint64 firstSampleMs = 0, lastSampleMs = 0;
    double peakUsedPercent;
    int risingRuns = 1;
    QuotaCycle(qint64 resetAtMs = 0, qint64 startMs = 0, double usedPercent = 0,
        qsizetype sampleCount = 0, double observedFraction = 0, qint64 firstSampleMs = 0,
        qint64 lastSampleMs = 0, std::optional<double> peakUsedPercent = std::nullopt, int risingRuns = 1)
        : resetAtMs(resetAtMs), startMs(startMs), usedPercent(usedPercent), sampleCount(sampleCount),
          observedFraction(observedFraction), firstSampleMs(firstSampleMs), lastSampleMs(lastSampleMs),
          peakUsedPercent(peakUsedPercent.value_or(usedPercent)), risingRuns(risingRuns) {}
    qint64 evidenceStartMs() const { return std::min(startMs, firstSampleMs); }
    qint64 durationMs() const { return resetAtMs - startMs; }
    bool operator==(const QuotaCycle &) const = default;
};
}
