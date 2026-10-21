#include "AttributedDailySeries.h"
#include "AttributionString.h"
#include "ModelGrouping.h"
#include <algorithm>
#include <tuple>

namespace syrtis::core {
ContributionClient ContributionClient::decode(const json::Decoder &d) {
    return {d.field<QString>("client"), d.field<QString>("modelId"), d.field<QString>("providerId"),
        d.field<TokenBreakdown>("tokens"), d.field<double>("cost"), d.field<qint64>("messages")};
}
Contribution::Totals Contribution::Totals::decode(const json::Decoder &d) {
    return {d.field<qint64>("tokens"), d.field<double>("cost"), d.field<qint64>("messages")};
}
Contribution Contribution::decode(const json::Decoder &d) {
    Contribution out{d.field<QString>("date"), d.field<Totals>("totals"), d.field<qint64>("intensity"),
        d.field<TokenBreakdown>("tokenBreakdown"), d.field<QList<ContributionClient>>("clients"), std::nullopt};
    const auto turns = d.value.toObject().value("turnsByClient");
    if (!turns.isNull() && !turns.isUndefined()) {
        const json::Decoder map{turns, d.path + QStringList{"turnsByClient"}};
        map.object();
        out.turnsByClient.emplace();
        const auto object = turns.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            (*out.turnsByClient)[it.key()] = json::Decoder{it.value(), map.path + QStringList{it.key()}}.decode<qint64>();
    }
    return out;
}
namespace AttributedDailySeries {
bool Point::operator==(const Point &other) const {
    return attributionKey(date) == attributionKey(other.date) && state == other.state
        && attributionKey(model) == attributionKey(other.model) && tokens == other.tokens && cost == other.cost;
}
QList<Point> points(const QList<Contribution> &contributions, const QList<UsageAttribution::Record> &confirmed) {
    QList<Point> rows;
    for (const auto &c : contributions) for (const auto &client : c.clients) {
        const auto tokens = client.tokens.total();
        if (tokens == 0 && client.cost == 0) continue;
        const auto state = client.providerId.contains(',') ? UsageAttribution::State{} :
            UsageAttribution::resolve(client.client, client.providerId, client.modelId, confirmed);
        rows.append({c.date, state, ModelGrouping::groupID(client.modelId), tokens, client.cost});
    }
    auto key = [](const Point &p) { return std::tuple{attributionKey(p.date).toUcs4(),
        attributionKey(attributionStateID(p.state)).toUcs4(), attributionKey(p.model).toUcs4()}; };
    std::stable_sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
        if (key(a) != key(b)) return key(a) < key(b);
        if (a.tokens != b.tokens) return a.tokens < b.tokens;
        return a.cost < b.cost;
    });
    QList<Point> out;
    for (const auto &r : rows) {
        if (out.isEmpty() || key(out.last()) != key(r)) out.append(r);
        else {
            out.last().tokens = saturatingAdding(out.last().tokens, r.tokens);
            out.last().cost += r.cost;
        }
    }
    out.removeIf([](const auto &p) { return p.tokens == 0 && p.cost == 0; });
    return out;
}
}
}
