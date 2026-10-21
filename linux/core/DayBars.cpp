#include "DayBars.h"
#include "UsageStats.h"
#include "ClientRegistry.h"
#include "ModelGrouping.h"
#include <QtCore/QMap>
#include <algorithm>

namespace syrtis::core {
qint64 DayBar::totalTokens() const
{
    qint64 result = 0;
    for (const auto &s : segments) result = saturatingAdding(result, s.tokens);
    return result;
}
double DayBar::totalCost() const
{
    double result = 0;
    for (const auto &s : segments) result += s.cost;
    return result;
}
DayBar DayBars::dayBar(const Contribution &contribution, const QSet<QString> &allowed, StackBy stackBy,
                       const ModelColorMap &colors)
{
    QMap<QString, DaySegment> grouped;
    for (const auto &client : contribution.clients) {
        if (!allowed.contains(client.client)) continue;
        const auto tokens = client.tokens.total();
        if (tokens <= 0 && client.cost <= 0) continue;
        const auto model = client.modelId.isEmpty() ? QString("unknown") : ModelGrouping::groupID(client.modelId);
        const auto key = stackBy == StackBy::model ? model : client.client;
        if (!grouped.contains(key)) {
            grouped[key] = stackBy == StackBy::model
                ? DaySegment{key, model, colors.color(client.providerId, model)}
                : DaySegment{key, ClientRegistry::shortName(client.client), ClientRegistry::style(client.client).color};
        }
        auto &slot = grouped[key];
        slot.tokens = saturatingAdding(slot.tokens, tokens);
        slot.cost += client.cost;
    }
    return {contribution.date, grouped.values()};
}
QList<DayBar> DayBars::build(const UsagePayload &payload, const QStringList &clientIds, StackBy stackBy,
                           const ModelColorMap &colors, const QString &rangeEnd, const QString &endFallback)
{
    const QSet<QString> allowed(clientIds.begin(), clientIds.end());
    QMap<QString, DayBar> byDate;
    for (const auto &c : payload.contributions) {
        const auto day = dayBar(c, allowed, stackBy, colors);
        if (day.totalTokens() > 0 || day.totalCost() > 0) byDate[day.date] = day;
    }
    const auto end = byDate.isEmpty() ? (rangeEnd.isEmpty() ? endFallback : rangeEnd) : byDate.lastKey();
    const auto endDay = ISODay::parse(end);
    if (!endDay) return {};
    QList<DayBar> bars;
    for (int i = 0; i < window; ++i) {
        const auto date = ISODay{endDay->number - (window - 1) + i}.iso();
        bars.append(byDate.value(date, DayBar{date, {}}));
    }
    return bars;
}
QList<DaySegment> DayBars::legend(const QList<DayBar> &bars, ChartMetric metric)
{
    QMap<QString, DaySegment> agg;
    for (const auto &bar : bars) {
        for (const auto &seg : bar.segments) {
            if (!agg.contains(seg.key)) agg[seg.key] = {seg.key, seg.label, seg.color};
            auto &slot = agg[seg.key];
            slot.tokens = saturatingAdding(slot.tokens, seg.tokens);
            slot.cost += seg.cost;
        }
    }
    auto result = agg.values();
    // Swift leaves equal-weight dictionary entries unordered; callers must not
    // assign a semantic order to ties (the oracle serializes tied legends by key).
    std::stable_sort(result.begin(), result.end(), [&](const auto &a, const auto &b) {
        return metric == ChartMetric::cost ? a.cost > b.cost : a.tokens > b.tokens;
    });
    return result;
}
}
