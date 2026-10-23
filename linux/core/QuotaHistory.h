#pragma once
#include "QuotaCurve.h"
#include "QuotaCycle.h"
#include "UsageAttribution.h"
#include "WindowUsage.h"

namespace syrtis::core {
struct QuotaHistoryModel {
    QString providerId, modelId;
    qint64 tokens = 0;
    TokenBreakdown breakdown;
    double cost = 0;
    QString id() const { return providerId + "|" + modelId; }
    bool operator==(const QuotaHistoryModel &) const = default;
};
struct QuotaHistoryRow {
    QuotaCycle cycle;
    qint64 mineTokens = 0, mineTokensExCacheRead = 0;
    TokenBreakdown mineBreakdown;
    double mineCost = 0;
    qint64 spanTokens = 0;
    double spanCost = 0;
    qint64 otherTokens = 0;
    double otherCost = 0;
    bool otherHasAssigned = false, otherHasExcluded = false, otherHasUnattributed = false;
    QList<QuotaHistoryModel> models;
    qint64 id() const { return cycle.resetAtMs; }
    bool operator==(const QuotaHistoryRow &) const = default;
};
namespace QuotaHistoryFold {
struct ModelKey {
    QString providerId, modelId;
    bool operator==(const ModelKey &) const = default;
};
inline constexpr qsizetype consideredCycles = 32;
struct Span { qint64 tokens = 0; double cost = 0; bool operator==(const Span &) const = default; };
qint64 canonicalReset(qint64 resetAt, qint64 durationSeconds);
QList<QuotaCycle> cycles(const QList<syrtis::core::QuotaCurvePoint> &points);
QList<QuotaCycle> considered(const QList<QuotaCycle> &cycles);
QList<QuotaHistoryRow> rows(const QList<QuotaCycle> &cycles, const QList<WindowMessage> &messages,
    const QString &subscription, const std::optional<QString> &modelScope, const QList<UsageAttribution::Record> &confirmed);
QList<Span> spans(const QList<QuotaCycle> &cycles, const QList<WindowMessage> &messages,
    const QString &subscription, const std::optional<QString> &modelScope, const QList<UsageAttribution::Record> &confirmed);
double consumed(const QList<double> &readings);
int risingRuns(const QList<double> &readings);
QList<WindowMessage> inScope(const QList<WindowMessage> &messages, const std::optional<QString> &scope);
qsizetype lowerBound(const QList<qint64> &values, qint64 value);
}
}
