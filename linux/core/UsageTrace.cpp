#include "UsageTrace.h"
#include "Graph.h"
#include "ClientRegistry.h"
#include <QtCore/QHash>
#include <algorithm>

namespace syrtis::core {
TraceBucket TraceBucket::decode(const json::Decoder &d)
{
    d.object();
    return {d.field<QString>("client"), d.field<QString>("agent"), d.field<QString>("model"),
            d.field<qint64>("tokens"), d.field<qint64>("messages"), d.field<double>("tokens_per_min")};
}
TokensPerMin TokensPerMin::decode(const json::Decoder &d)
{
    d.object();
    return {d.field<double>("tokensPerMin")};
}
double TraceBucket::totalRate(const QList<TraceBucket> &buckets, const QSet<QString> &hidden)
{
    double rate = 0;
    for (const auto &b : buckets)
        if (!hidden.contains(ClientRegistry::canonicalClient(b.client))) rate += b.tokensPerMin;
    return rate;
}
QList<TraceBucket> TraceBucket::collapseByClient(const QList<TraceBucket> &buckets)
{
    struct Slot {
        qint64 tokens = 0;
        qint64 messages = 0;
        double rate = 0;
        QSet<QString> agents, models;
    };
    QHash<QString, Slot> groups;
    QStringList order;
    for (const auto &b : buckets) {
        if (!groups.contains(b.client)) order.append(b.client);
        auto &slot = groups[b.client];
        slot.tokens = saturatingAdding(slot.tokens, b.tokens);
        slot.messages += b.messages;
        slot.rate += b.tokensPerMin;
        slot.agents.insert(b.agent);
        slot.models.insert(b.model);
    }
    QList<TraceBucket> result;
    for (const auto &client : order) {
        const auto &slot = groups[client];
        auto models = slot.models.values(), agents = slot.agents.values();
        std::sort(models.begin(), models.end());
        std::sort(agents.begin(), agents.end());
        if (models.size() > 1) models.removeAll("unknown");
        result.append({client, agents.join(", "), models.join(", "), slot.tokens, slot.messages, slot.rate});
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.tokens > b.tokens; });
    return result;
}
}
