#pragma once
#include "UsageAttribution.h"
#include "WindowCardGeometry.h"
#include <functional>

namespace syrtis::core::WindowEquivalence {
inline constexpr double quantisationHalfStep = 0.5, tolerance = 0.10;
inline constexpr double minimumDelta = quantisationHalfStep / tolerance;
inline constexpr double minimumObservedFraction = 0.5;
inline constexpr int minimumCycles = 3;
struct Row {
    enum class Kind { ratio, insufficient, notMoved, unaccounted, unavailable, costOnly,
                      tokensOnly, tooFewCycles, undeclared, spread };
    Kind kind = Kind::unavailable;
    qint64 tokensPerTenth = 0;
    double costPerTenth = 0;
    int errorPercent = 0;
    double deltaPercent = 0;
    qsizetype count = 0;
    int needed = 0;
    qint64 lowPerTenth = 0, highPerTenth = 0;
    double lowCostPerTenth = 0, highCostPerTenth = 0;
    bool isRatio() const { return kind == Kind::ratio; }
    bool operator==(const Row &other) const;
};
struct Cycle {
    double deltaPercent;
    qint64 spanTokens;
    double spanCost, observedFraction;
    int risingRuns = 1;
    bool operator==(const Cycle &) const = default;
};
qint64 ratioTokens(const WindowMessage &message);
bool deltaQualifies(double delta, int runs);
qint64 clamped(double value);
QString text(const Row &row, const std::function<QString(qint64)> &tokens, const std::function<QString(double)> &money);
Row row(const QList<QuotaSample> &samples, const QList<WindowMessage> &messages);
Row aggregate(const QList<Cycle> &cycles, bool declared = true);
Row aggregate(const QString &subscription, const QList<UsageAttribution::Record> &records, const QList<Cycle> &cycles);
}
