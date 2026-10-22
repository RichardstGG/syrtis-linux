#include "WindowEquivalence.h"
#include "QuotaHistory.h"
#include "Localization.h"
#include <algorithm>
#include <numeric>

namespace syrtis::core::WindowEquivalence {
bool Row::operator==(const Row &r) const {
    if (kind != r.kind) return false;
    // Match Swift's enum equality: only the active case's payload participates.
    switch (kind) {
    case Kind::ratio: return tokensPerTenth == r.tokensPerTenth && costPerTenth == r.costPerTenth && errorPercent == r.errorPercent;
    case Kind::insufficient: return deltaPercent == r.deltaPercent && errorPercent == r.errorPercent;
    case Kind::unaccounted: return deltaPercent == r.deltaPercent;
    case Kind::costOnly: return costPerTenth == r.costPerTenth && errorPercent == r.errorPercent;
    case Kind::tokensOnly: return tokensPerTenth == r.tokensPerTenth && errorPercent == r.errorPercent;
    case Kind::tooFewCycles: return count == r.count && needed == r.needed;
    case Kind::spread: return lowPerTenth == r.lowPerTenth && highPerTenth == r.highPerTenth
        && lowCostPerTenth == r.lowCostPerTenth && highCostPerTenth == r.highCostPerTenth;
    case Kind::notMoved: case Kind::unavailable: case Kind::undeclared: return true;
    }
    return false;
}
qint64 ratioTokens(const WindowMessage &m) { return m.tokens(); }
bool deltaQualifies(double delta, int runs) { return delta > 0 && delta >= minimumDelta * double(runs); }
qint64 clamped(double value) {
    if (!std::isfinite(value)) return 0;
    if (value >= double(std::numeric_limits<qint64>::max())) return std::numeric_limits<qint64>::max();
    if (value <= double(std::numeric_limits<qint64>::min())) return std::numeric_limits<qint64>::min();
    return qint64(value);
}
QString text(const Row &r, const std::function<QString(qint64)> &tokens, const std::function<QString(double)> &money) {
    using Syrtis::localized;
    const auto error = QString::number(r.errorPercent);
    switch (r.kind) {
    case Row::Kind::ratio: return localized("10%% of quota ~ %@ · %@ API-equivalent, ±%@%%", {tokens(r.tokensPerTenth), money(r.costPerTenth), error});
    case Row::Kind::insufficient: return localized("Quota moved only %@%% — too little to estimate (±%@%%)", {QString::number(qint64(std::round(r.deltaPercent))), error});
    case Row::Kind::notMoved: return localized("Quota has not moved yet");
    case Row::Kind::unaccounted: return localized("Quota moved %@%%, none of it recorded on this machine", {QString::number(qint64(std::round(r.deltaPercent)))});
    case Row::Kind::unavailable: return localized("Not enough quota readings yet");
    case Row::Kind::costOnly: return localized("10%% of quota ~ %@ API-equivalent, tokens unavailable, ±%@%%", {money(r.costPerTenth), error});
    case Row::Kind::tokensOnly: return localized("10%% of quota ~ %@, unpriced models, ±%@%%", {tokens(r.tokensPerTenth), error});
    case Row::Kind::tooFewCycles: return localized("%@ of %@ windows recorded — the estimate needs that many", {QString::number(r.count), QString::number(r.needed)});
    case Row::Kind::undeclared: return localized("Classify your usage in Settings to see what this window is worth");
    case Row::Kind::spread: return localized("10%% of quota ~ %@-%@ · %@-%@ API-equivalent", {tokens(r.lowPerTenth), tokens(r.highPerTenth), money(r.lowCostPerTenth), money(r.highCostPerTenth)});
    }
    return {};
}
Row row(const QList<QuotaSample> &samples, const QList<WindowMessage> &messages) {
    if (samples.size() < 2) return {};
    QList<double> readings;
    for (const auto &s : samples) readings.append(s.usedPercent);
    const auto delta = QuotaHistoryFold::consumed(readings);
    if (delta <= 0) return {Row::Kind::notMoved};
    const auto runs = QuotaHistoryFold::risingRuns(readings);
    qint64 tokens = 0;
    double cost = 0;
    for (const auto &m : messages) if (m.timestamp > samples.first().atMs && m.timestamp <= samples.last().atMs) {
        tokens = saturatingAdding(tokens, ratioTokens(m));
        cost += m.cost;
    }
    Row r;
    r.deltaPercent = delta;
    r.errorPercent = int(std::round(quantisationHalfStep * double(runs) / delta * 100));
    if (tokens <= 0 && cost <= 0) { r.kind = Row::Kind::unaccounted; return r; }
    if (!deltaQualifies(delta, runs)) { r.kind = Row::Kind::insufficient; return r; }
    r.tokensPerTenth = clamped(std::round(double(tokens) / delta * 10));
    r.costPerTenth = cost / delta * 10;
    r.kind = cost <= 0 ? Row::Kind::tokensOnly : tokens <= 0 ? Row::Kind::costOnly : Row::Kind::ratio;
    return r;
}
Row aggregate(const QString &subscription, const QList<UsageAttribution::Record> &records, const QList<Cycle> &cycles) {
    return aggregate(cycles, UsageAttribution::declares(subscription, records));
}
Row aggregate(const QList<Cycle> &cycles, bool declared) {
    if (!declared) return {Row::Kind::undeclared};
    QList<Cycle> admitted;
    for (const auto &c : cycles)
        if (deltaQualifies(c.deltaPercent, c.risingRuns) && c.observedFraction >= minimumObservedFraction
            && (c.spanCost > 0 || c.spanTokens > 0)) admitted.append(c);
    if (admitted.isEmpty()) {
        if (cycles.isEmpty()) return {};
        double movement = 0;
        int runs = 0;
        bool empty = true;
        for (const auto &c : cycles) { movement += c.deltaPercent; runs += c.risingRuns; empty &= c.spanCost <= 0 && c.spanTokens <= 0; }
        if (movement <= 0) return {Row::Kind::notMoved};
        Row r;
        r.kind = empty ? Row::Kind::unaccounted : Row::Kind::insufficient;
        r.deltaPercent = movement;
        r.errorPercent = int(std::round(quantisationHalfStep * double(runs) / movement * 100));
        return r;
    }
    auto tooFew = [](qsizetype count) { Row r{Row::Kind::tooFewCycles}; r.count = count; r.needed = minimumCycles; return r; };
    if (admitted.size() < minimumCycles) return tooFew(admitted.size());
    auto pooled = [](const QList<Cycle> &set, const auto &pick) {
        double delta = 0, total = 0;
        for (const auto &c : set) { delta += c.deltaPercent; total += pick(c); }
        return delta > 0 ? total / delta : 0;
    };
    auto jackknife = [&](const QList<Cycle> &set, const auto &pick) {
        const auto estimate = pooled(set, pick);
        if (set.size() < minimumCycles || estimate <= 0) return std::numeric_limits<double>::infinity();
        const auto n = double(set.size());
        QList<double> leaveOneOut;
        double mean = 0;
        for (qsizetype i = 0; i < set.size(); ++i) {
            auto rest = set;
            rest.removeAt(i);
            const auto v = pooled(rest, pick);
            leaveOneOut.append(v);
            mean += v;
        }
        mean /= n;
        double sum = 0;
        for (auto v : leaveOneOut) sum += (v - mean) * (v - mean);
        return std::sqrt((n - 1) / n * sum) / estimate;
    };
    QList<Cycle> priced, tokenBearing;
    for (const auto &c : admitted) {
        if (c.spanCost > 0) priced.append(c);
        if (c.spanTokens > 0) tokenBearing.append(c);
    }
    const auto cost = [](const Cycle &c) { return c.spanCost; };
    const auto tokens = [](const Cycle &c) { return double(c.spanTokens); };
    Row r;
    r.costPerTenth = pooled(priced, cost) * 10;
    r.tokensPerTenth = clamped(std::round(pooled(tokenBearing, tokens) * 10));
    const auto errorPercent = [](double error) { return std::isfinite(error) ? std::max(1, int(std::round(error * 100))) : 0; };
    if (priced.size() < minimumCycles) {
        if (tokenBearing.size() < minimumCycles) return tooFew(std::max(priced.size(), tokenBearing.size()));
        r.kind = Row::Kind::tokensOnly;
        r.errorPercent = errorPercent(jackknife(tokenBearing, tokens));
        return r;
    }
    if (tokenBearing.size() < minimumCycles) {
        r.kind = Row::Kind::costOnly;
        r.errorPercent = errorPercent(jackknife(priced, cost));
        return r;
    }
    const auto relativeError = std::max(jackknife(priced, cost), jackknife(tokenBearing, tokens));
    if (relativeError > tolerance) {
        r.kind = Row::Kind::spread;
        double low = std::numeric_limits<double>::infinity(), high = 0;
        for (const auto &c : tokenBearing) { const auto v = double(c.spanTokens) / c.deltaPercent * 10; low = std::min(low, v); high = std::max(high, v); }
        r.lowPerTenth = clamped(std::round(low)); r.highPerTenth = clamped(std::round(high));
        r.lowCostPerTenth = std::numeric_limits<double>::infinity();
        for (const auto &c : priced) { const auto v = c.spanCost / c.deltaPercent * 10; r.lowCostPerTenth = std::min(r.lowCostPerTenth, v); r.highCostPerTenth = std::max(r.highCostPerTenth, v); }
        return r;
    }
    r.kind = Row::Kind::ratio;
    r.errorPercent = std::max(1, int(std::round(relativeError * 100)));
    return r;
}
}
