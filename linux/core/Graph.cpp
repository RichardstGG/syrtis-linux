#include "Graph.h"
#include <QtCore/QMap>

namespace syrtis::core {
TokenBreakdown TokenBreakdown::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
    };
}
ContributionClient ContributionClient::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("client"),
        d.field<QString>("modelId"),
        d.field<QString>("providerId"),
        d.field<TokenBreakdown>("tokens"),
        d.field<double>("cost"),
        d.field<qint64>("messages"),
    };
}
ContributionTotals ContributionTotals::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("tokens"),
        d.field<double>("cost"),
        d.field<qint64>("messages"),
    };
}
Contribution Contribution::decode(const json::Decoder &d)
{
    d.object();
    auto result = Contribution{
        d.field<QString>("date"),
        d.field<ContributionTotals>("totals"),
        d.field<qint64>("intensity"),
        d.field<TokenBreakdown>("tokenBreakdown"),
        d.field<QList<ContributionClient>>("clients"),
        std::nullopt};
    const auto turns = d.value.toObject()["turnsByClient"];
    if (!turns.isNull() && !turns.isUndefined()) {
        const json::Decoder map{turns, d.path + QStringList{"turnsByClient"}};
        map.object();
        QHash<QString, qint64> counts;
        const auto object = turns.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            counts.insert(it.key(), map.field<qint64>(it.key()));
        result.turnsByClient = counts;
    }
    return result;
}
DateRange DateRange::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("start"),
        d.field<QString>("end"),
    };
}
YearMeta YearMeta::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("year"),
        d.field<qint64>("totalTokens"),
        d.field<double>("totalCost"),
        d.field<DateRange>("range"),
    };
}
UsageMeta UsageMeta::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("generatedAt"),
        d.field<QString>("version"),
        d.field<DateRange>("dateRange"),
    };
}
UsageSummary UsageSummary::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("totalTokens"),
        d.field<double>("totalCost"),
        d.field<qint64>("totalDays"),
        d.field<qint64>("activeDays"),
        d.field<double>("averagePerDay"),
        d.field<double>("maxCostInSingleDay"),
        d.field<QList<QString>>("clients"),
        d.field<QList<QString>>("models"),
    };
}
UsagePayload UsagePayload::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<UsageMeta>("meta"),
        d.field<UsageSummary>("summary"),
        d.field<QList<YearMeta>>("years"),
        d.field<QList<Contribution>>("contributions"),
    };
}
}

namespace syrtis::core {
qint64 saturatingAdding(qint64 a, qint64 b)
{
    if (b > 0 && a > std::numeric_limits<qint64>::max() - b)
        return std::numeric_limits<qint64>::max();
    if (b < 0 && a < std::numeric_limits<qint64>::min() - b)
        return std::numeric_limits<qint64>::min();
    return a + b;
}
qint64 TokenBreakdown::total() const
{
    return saturatingAdding(saturatingAdding(saturatingAdding(
        saturatingAdding(input, output), cacheRead), cacheWrite), reasoning);
}
std::optional<qint64> Contribution::turns(const QStringList &clients) const
{
    if (clients.isEmpty()) return std::nullopt;
    qint64 count = 0;
    for (const auto &id : clients)
        count = saturatingAdding(count, turnsByClient ? turnsByClient->value(id) : 0);
    return count;
}
static bool survives(const QString &id, const QSet<QString> &hidden,
                     const std::optional<QSet<QString>> &only)
{
    return (!only || only->contains(id)) && !hidden.contains(id);
}
std::optional<QString> UsagePayload::topVisibleClient(const QList<ContributionClient> &clients,
    const QSet<QString> &hidden, const std::optional<QSet<QString>> &only)
{
    struct Total { qint64 tokens = 0; double cost = 0; };
    QMap<QString, Total> byClient;
    for (const auto &cc : clients) {
        if (!survives(cc.client, hidden, only)) continue;
        auto &slot = byClient[cc.client];
        slot.tokens = saturatingAdding(slot.tokens, cc.tokens.total());
        slot.cost += cc.cost;
    }
    std::optional<QString> best;
    Total top;
    for (auto it = byClient.begin(); it != byClient.end(); ++it) {
        if (!best || it->tokens > top.tokens || (it->tokens == top.tokens && it->cost > top.cost)) {
            best = it.key();
            top = it.value();
        }
    }
    return best;
}
TrayTotals UsagePayload::trayTotals(const QSet<QString> &hidden, const QString &today,
    const std::optional<QSet<QString>> &only) const
{
    const Contribution *todayEntry = nullptr;
    for (const auto &c : contributions) if (c.date == today) todayEntry = &c;
    const auto top = topVisibleClient(todayEntry ? todayEntry->clients : QList<ContributionClient>{}, hidden, only);
    if (hidden.isEmpty() && !only)
        return {todayEntry ? todayEntry->totals.tokens : 0, todayEntry ? todayEntry->totals.cost : 0,
                summary.totalTokens, summary.totalCost, top};
    TrayTotals result{0, 0, 0, 0, top};
    for (const auto &c : contributions) {
        for (const auto &cc : c.clients) {
            if (!survives(cc.client, hidden, only)) continue;
            result.totalTokens = saturatingAdding(result.totalTokens, cc.tokens.total());
            result.totalCost += cc.cost;
            if (c.date == today) {
                result.todayTokens = saturatingAdding(result.todayTokens, cc.tokens.total());
                result.todayCost += cc.cost;
            }
        }
    }
    return result;
}
}
