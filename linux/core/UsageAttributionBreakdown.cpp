#include "UsageAttributionBreakdown.h"
#include "AttributionString.h"
#include "TokenBreakdown.h"
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <algorithm>

namespace syrtis::core::UsageAttributionBreakdown {
namespace Copy {
const QString title = "API-list-price equivalent";
const QString hint = "Nothing is classified yet. Classify sources in Settings → Usage attribution.";
const QString noUsage = "No attributed usage in this range.";
const QString unavailable = "Usage breakdown is unavailable right now.";
QStringList all() { return {title, hint, noUsage, unavailable}; }
}
QString Row::id() const { return attributionStateID(state); }
QList<Row> rows(const QList<ModelReportEntry> &entries, const QStringList &clientIds,
                const QList<UsageAttribution::Record> &confirmed) {
    using Kind = UsageAttribution::State::Kind;
    QSet<QString> allowed;
    for (const auto &id : clientIds) allowed.insert(attributionKey(id));
    QMap<QString, Row> assigned;
    Row excluded{{Kind::excluded, {}}, 0, 0}, unassigned{{}, 0, 0};
    for (const auto &e : entries) {
        if (!allowed.contains(attributionKey(e.client)) || (e.total == 0 && e.cost == 0)) continue;
        const auto state = UsageAttribution::resolve(e, confirmed);
        Row *r;
        if (state.kind == Kind::assigned) {
            const auto key = attributionKey(state.target);
            if (!assigned.contains(key)) assigned[key] = {state, 0, 0};
            r = &assigned[key];
        } else r = state.kind == Kind::excluded ? &excluded : &unassigned;
        r->tokens = saturatingAdding(r->tokens, e.total);
        r->cost += e.cost;
    }
    auto result = assigned.values();
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return attributionLess(a.state.target, b.state.target); });
    result.append(excluded); result.append(unassigned);
    result.removeIf([](const auto &r) { return r.tokens == 0 && r.cost == 0; });
    return result;
}
}
