#include "SubscriptionTrend.h"
#include "AttributionString.h"
#include <QtCore/QDate>
#include <algorithm>

namespace syrtis::core {
const SubscriptionTrend SubscriptionTrend::empty{};
bool SubscriptionTrend::Day::operator==(const Day &other) const {
    auto normalized = [](const auto &buckets) {
        QMap<QString, Bucket> result;
        for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) result[attributionKey(it.key())] = it.value();
        return result;
    };
    return attributionKey(date) == attributionKey(other.date) && normalized(byTarget) == normalized(other.byTarget)
        && totalTokens == other.totalTokens && totalCost == other.totalCost;
}
bool SubscriptionTrend::operator==(const SubscriptionTrend &other) const {
    auto normalized = [](QStringList list) {
        for (auto &value : list) value = attributionKey(value);
        return list;
    };
    return days == other.days && normalized(targets) == normalized(other.targets)
        && normalized(targetsByTokens) == normalized(other.targetsByTokens) && peakCost == other.peakCost && peakTokens == other.peakTokens;
}
namespace SubscriptionTrendFold {
std::optional<QStringList> calendarRange(const QString &endingAt, qsizetype count) {
    const auto end = QDate::fromString(endingAt, "yyyy-MM-dd");
    if (!end.isValid()) return std::nullopt;
    QStringList out;
    // Civil-date arithmetic preserves calendar days across DST transitions.
    for (qsizetype back = count; back > 0; --back) {
        const auto date = end.addDays(-(back - 1));
        if (date.isValid()) out.append(date.toString("yyyy-MM-dd"));
    }
    return out;
}
SubscriptionTrend build(const QList<AttributedDailySeries::Point> &points, const QString &today, qsizetype days) {
    if (days <= 0) return SubscriptionTrend::empty;
    const auto range = calendarRange(today, days);
    if (!range || range->isEmpty()) return SubscriptionTrend::empty;
    QMap<QString, QMap<QString, SubscriptionTrend::Bucket>> byDate;
    QMap<QString, double> totals;
    QMap<QString, qint64> tokenTotals;
    QMap<QString, QString> names;
    QMap<QString, QMap<QString, QString>> dayNames;
    for (const auto &p : points) {
        if (attributionLess(p.date, range->first()) || attributionLess(today, p.date)
            || p.state.kind == UsageAttribution::State::Kind::excluded) continue;
        const auto target = p.state.kind == UsageAttribution::State::Kind::assigned ? p.state.target : unassignedTarget;
        const auto key = attributionKey(target);
        if (!names.contains(key)) names[key] = target;
        const auto dateKey = attributionKey(p.date);
        if (!dayNames[dateKey].contains(key)) dayNames[dateKey][key] = target;
        auto &bucket = byDate[dateKey][key];
        bucket.tokens = saturatingAdding(bucket.tokens, p.tokens);
        bucket.cost += p.cost;
        totals[key] += p.cost;
        tokenTotals[key] = saturatingAdding(tokenTotals.value(key), p.tokens);
    }
    SubscriptionTrend out;
    for (const auto &date : *range) {
        SubscriptionTrend::Day day{date, {}};
        const auto buckets = byDate.value(attributionKey(date));
        for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
            day.byTarget[dayNames[attributionKey(date)][it.key()]] = it.value();
            day.totalTokens = saturatingAdding(day.totalTokens, it->tokens);
            day.totalCost += it->cost;
        }
        out.days.append(day);
    }
    auto costKeys = totals.keys(), tokenKeys = tokenTotals.keys();
    std::sort(costKeys.begin(), costKeys.end(), [&](const auto &a, const auto &b) {
        return totals[a] == totals[b] ? attributionLess(a, b) : totals[a] > totals[b];
    });
    std::sort(tokenKeys.begin(), tokenKeys.end(), [&](const auto &a, const auto &b) {
        return tokenTotals[a] == tokenTotals[b] ? attributionLess(a, b) : tokenTotals[a] > tokenTotals[b];
    });
    for (const auto &key : costKeys) out.targets.append(names[key]);
    for (const auto &key : tokenKeys) out.targetsByTokens.append(names[key]);
    out.peakCost = out.days.first().totalCost;
    out.peakTokens = out.days.first().totalTokens;
    for (const auto &day : out.days) {
        out.peakCost = std::max(out.peakCost, day.totalCost);
        out.peakTokens = std::max(out.peakTokens, day.totalTokens);
    }
    return out;
}
}
}
