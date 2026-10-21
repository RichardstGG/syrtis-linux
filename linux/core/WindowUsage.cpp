#include "WindowUsage.h"
#include "AttributionString.h"
#include <QtCore/QMap>
#include <algorithm>

namespace syrtis::core {
namespace {
// WindowMessage deliberately clamps either overflow direction to Int64.max,
// unlike the signed saturation used by TokenBreakdown and the folds.
qint64 messageAdd(qint64 a, qint64 b) {
    if ((b > 0 && a > std::numeric_limits<qint64>::max() - b)
        || (b < 0 && a < std::numeric_limits<qint64>::min() - b))
        return std::numeric_limits<qint64>::max();
    return a + b;
}
}
qint64 WindowMessage::tokens() const {
    return messageAdd(messageAdd(messageAdd(messageAdd(input, output), cacheRead), cacheWrite), reasoning);
}
qint64 WindowMessage::tokensExCacheRead() const {
    return messageAdd(messageAdd(messageAdd(input, output), cacheWrite), reasoning);
}
WindowMessage WindowMessage::decode(const json::Decoder &d) {
    return {d.field<qint64>("timestamp"), d.field<QString>("client"),
        d.field<QString>("providerId"), d.field<QString>("modelId"),
        d.field<qint64>("input"), d.field<qint64>("output"), d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"), d.field<qint64>("reasoning"),
        d.field<double>("cost"), d.field<bool>("isTurnStart")};
}

bool WindowUsage::SubscriptionTotal::operator==(const SubscriptionTotal &other) const {
    return attributionKey(target) == attributionKey(other.target) && tokens == other.tokens && cost == other.cost;
}
WindowUsage WindowUsage::decode(const json::Decoder &d) {
    return {d.field<QList<WindowMessage>>("messages"), d.field<qint64>("undatedCount"), d.field<qint64>("processingTimeMs")};
}
WindowUsage::Totals WindowUsage::totals(const QList<UsageAttribution::Record> &confirmed) const {
    Totals out;
    QMap<QString, SubscriptionTotal> assigned;
    for (const auto &m : messages) {
        const auto state = UsageAttribution::resolve(m.client, m.providerId, m.modelId, confirmed);
        SubscriptionTotal *bucket;
        if (state.kind == UsageAttribution::State::Kind::assigned) {
            auto &value = assigned[attributionKey(state.target)];
            if (value.target.isNull()) value.target = state.target;
            bucket = &value;
        } else bucket = state.kind == UsageAttribution::State::Kind::excluded ? &out.excluded : &out.unassigned;
        bucket->tokens = saturatingAdding(bucket->tokens, m.tokens());
        bucket->cost += m.cost;
    }
    out.assigned = assigned.values();
    // Swift sorts only by tokens; equal-token targets have no ordering contract.
    std::stable_sort(out.assigned.begin(), out.assigned.end(), [](const auto &a, const auto &b) { return a.tokens > b.tokens; });
    return out;
}
QList<qint64> WindowUsage::buckets(qint64 from, qint64 bucketMs, qsizetype count) const {
    QList<qint64> out(std::max(qsizetype(0), count), 0);
    if (bucketMs <= 0 || count <= 0) return out;
    for (const auto &m : messages) {
        // Widen the difference to avoid C++ signed overflow at transcript bounds.
        // Division truncates toward zero, including timestamps just before from.
        const auto index = (__int128(m.timestamp) - from) / bucketMs;
        if (index >= 0 && index < count) out[qsizetype(index)] = saturatingAdding(out[qsizetype(index)], m.tokens());
    }
    return out;
}
}
