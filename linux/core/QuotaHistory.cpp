#include "QuotaHistory.h"
#include "ModelGrouping.h"
#include "ModelScope.h"
#include "WindowEquivalence.h"
#include <algorithm>
#include <map>

namespace syrtis::core::QuotaHistoryFold {
qint64 canonicalReset(qint64 resetAt, qint64 duration) {
    const auto quantum = std::max(std::min(std::max(duration / 100, qint64(0)), qint64(300)), qint64(60));
    const auto quotient = resetAt / quantum, remainder = resetAt % quantum;
    return (remainder * 2 >= quantum ? quotient + 1 : quotient) * quantum;
}
QList<QuotaCycle> cycles(const QList<Syrtis::QuotaCurvePoint> &points) {
    std::map<qint64, QList<Syrtis::QuotaCurvePoint>> grouped;
    for (const auto &p : points) if (!p.isActiveGroup) grouped[canonicalReset(p.resetAt, p.durationSeconds)].append(p);
    QList<QuotaCycle> out;
    for (auto &[key, sorted] : grouped) {
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.sampledAt < b.sampledAt; });
        const auto &first = sorted.first(), &last = sorted.last();
        if (last.durationSeconds <= 0) continue;
        std::map<qint64, int> tally;
        QList<double> used;
        for (const auto &p : sorted) { ++tally[p.resetAt]; used.append(p.usedPercent); }
        auto mode = tally.begin();
        for (auto it = tally.begin(); it != tally.end(); ++it) if (it->second > mode->second) mode = it;
        const auto reset = mode->first;
        out.append(QuotaCycle(reset * 1000, (reset - last.durationSeconds) * 1000, consumed(used), sorted.size(),
            std::clamp(double(last.sampledAt - first.sampledAt) / double(last.durationSeconds), 0.0, 1.0),
            first.sampledAt * 1000, last.sampledAt * 1000, *std::max_element(used.begin(), used.end()), risingRuns(used)));
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.resetAtMs > b.resetAtMs; });
    return out;
}
QList<QuotaCycle> considered(const QList<QuotaCycle> &cycles) { return cycles.first(std::min(cycles.size(), consideredCycles)); }
double consumed(const QList<double> &readings) {
    double out = 0;
    for (qsizetype i = 1; i < readings.size(); ++i) out += std::max(0.0, readings[i] - readings[i - 1]);
    return out;
}
int risingRuns(const QList<double> &readings) {
    int runs = 0;
    bool negative = true;
    for (qsizetype i = 1; i < readings.size(); ++i) {
        if (readings[i] > readings[i - 1]) { if (negative) ++runs; negative = false; }
        else if (readings[i] < readings[i - 1]) negative = true;
    }
    return runs;
}
QList<WindowMessage> inScope(const QList<WindowMessage> &messages, const std::optional<QString> &scope) {
    if (!scope) return messages;
    QHash<QString, bool> verdicts;
    QList<WindowMessage> out;
    for (const auto &m : messages) {
        if (!verdicts.contains(m.modelId)) verdicts[m.modelId] = ModelScope::covers(*scope, m.modelId);
        if (verdicts[m.modelId]) out.append(m);
    }
    return out;
}
qsizetype lowerBound(const QList<qint64> &values, qint64 value) {
    return std::lower_bound(values.begin(), values.end(), value) - values.begin();
}
namespace {
struct SortedMessages {
    QList<WindowMessage> messages;
    QList<qint64> stamps;
    SortedMessages(const QList<WindowMessage> &input, const std::optional<QString> &scope) : messages(inScope(input, scope)) {
        std::stable_sort(messages.begin(), messages.end(), [](const auto &a, const auto &b) { return a.timestamp < b.timestamp; });
        for (const auto &m : messages) stamps.append(m.timestamp);
    }
};
bool mine(const UsageAttribution::State &state, const QString &subscription) {
    return state.kind == UsageAttribution::State::Kind::assigned
        && state.target.normalized(QString::NormalizationForm_C) == subscription.normalized(QString::NormalizationForm_C);
}
QList<Span> spanTotals(const QList<QuotaCycle> &cycles, const SortedMessages &sorted,
                     const QString &subscription, const QList<UsageAttribution::Record> &confirmed) {
    QList<Span> out;
    for (const auto &c : cycles) {
        const auto lo = lowerBound(sorted.stamps, c.firstSampleMs);
        const auto hi = lowerBound(sorted.stamps, saturatingAdding(c.lastSampleMs, 1));
        Span span;
        for (auto i = lo; i < hi; ++i) {
            const auto &m = sorted.messages[i];
            if (m.timestamp <= c.firstSampleMs || m.timestamp > c.lastSampleMs
                || !mine(UsageAttribution::resolve(m.client, m.providerId, m.modelId, confirmed), subscription)) continue;
            span.tokens = saturatingAdding(span.tokens, WindowEquivalence::ratioTokens(m));
            span.cost += m.cost;
        }
        out.append(span);
    }
    return out;
}
void addSplit(TokenBreakdown &b, const WindowMessage &m) {
    b.input = saturatingAdding(b.input, m.input);
    b.output = saturatingAdding(b.output, m.output);
    b.cacheRead = saturatingAdding(b.cacheRead, m.cacheRead);
    b.cacheWrite = saturatingAdding(b.cacheWrite, m.cacheWrite);
    b.reasoning = saturatingAdding(b.reasoning, m.reasoning);
}
}
QList<Span> spans(const QList<QuotaCycle> &cycles, const QList<WindowMessage> &messages,
    const QString &subscription, const std::optional<QString> &scope, const QList<UsageAttribution::Record> &confirmed) {
    return spanTotals(cycles, SortedMessages(messages, scope), subscription, confirmed);
}
QList<QuotaHistoryRow> rows(const QList<QuotaCycle> &cycles, const QList<WindowMessage> &messages,
    const QString &subscription, const std::optional<QString> &scope, const QList<UsageAttribution::Record> &confirmed) {
    const SortedMessages sorted(messages, scope);
    const auto totals = spanTotals(cycles, sorted, subscription, confirmed);
    QList<QuotaHistoryRow> out;
    for (qsizetype index = 0; index < cycles.size(); ++index) {
        const auto &c = cycles[index];
        const auto lo = lowerBound(sorted.stamps, c.evidenceStartMs()), hi = lowerBound(sorted.stamps, c.resetAtMs);
        QuotaHistoryRow r;
        r.cycle = c;
        r.spanTokens = totals[index].tokens;
        r.spanCost = totals[index].cost;
        std::map<std::pair<QString, QString>, QuotaHistoryModel> models;
        for (auto i = lo; i < hi; ++i) {
            const auto &m = sorted.messages[i];
            const auto state = UsageAttribution::resolve(m.client, m.providerId, m.modelId, confirmed);
            if (mine(state, subscription)) {
                r.mineTokens = saturatingAdding(r.mineTokens, m.tokens());
                r.mineTokensExCacheRead = saturatingAdding(r.mineTokensExCacheRead, m.tokensExCacheRead());
                addSplit(r.mineBreakdown, m);
                r.mineCost += m.cost;
                const auto group = ModelGrouping::groupID(m.modelId);
                const auto key = std::pair{m.providerId.normalized(QString::NormalizationForm_C), group.normalized(QString::NormalizationForm_C)};
                auto [it, inserted] = models.try_emplace(key, QuotaHistoryModel{m.providerId, group});
                auto &model = it->second;
                model.tokens = saturatingAdding(model.tokens, m.tokens());
                model.cost += m.cost;
                addSplit(model.breakdown, m);
            } else {
                switch (state.kind) {
                case UsageAttribution::State::Kind::assigned: r.otherHasAssigned = true; break;
                case UsageAttribution::State::Kind::excluded: r.otherHasExcluded = true; break;
                case UsageAttribution::State::Kind::unassigned: r.otherHasUnattributed = true; break;
                }
                r.otherTokens = saturatingAdding(r.otherTokens, m.tokens());
                r.otherCost += m.cost;
            }
        }
        for (const auto &[key, model] : models) r.models.append(model);
        std::sort(r.models.begin(), r.models.end(), [](const auto &a, const auto &b) {
            if (a.tokens != b.tokens) return a.tokens > b.tokens;
            if (a.cost != b.cost) return a.cost > b.cost;
            const auto key = [](const auto &m) { return std::pair{m.providerId.normalized(QString::NormalizationForm_C).toUcs4(), m.modelId.normalized(QString::NormalizationForm_C).toUcs4()}; };
            return key(a) < key(b);
        });
        out.append(r);
    }
    return out;
}
}
