#include "attribution-ext.h"
#include "UsageAttributionSettings.h"
#include "UsageAttributionBreakdown.h"
#include "SubscriptionTrend.h"
#include "WindowUsage.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSet>

using namespace syrtis::core;
namespace A = UsageAttribution;
namespace S = UsageAttributionSettings;
namespace {
QJsonValue wire(const A::State &s) {
    QJsonObject out{{"kind", s.kind == A::State::Kind::assigned ? "assigned" : s.kind == A::State::Kind::excluded ? "excluded" : "unassigned"}};
    if (s.kind == A::State::Kind::assigned) out["target"] = s.target;
    return out;
}
QJsonValue wire(const A::Record &r) {
    return QJsonObject{{"client", r.client}, {"provider", r.provider}, {"model", r.model ? QJsonValue(*r.model) : QJsonValue(QJsonValue::Null)}, {"state", wire(r.state)}};
}
QJsonValue wire(const S::Row &r) {
    return QJsonObject{{"client", r.client}, {"provider", r.provider}, {"tokens", r.tokens}, {"cost", r.cost}, {"state", wire(r.state)},
        {"suggestedState", r.suggestedState ? wire(*r.suggestedState) : QJsonValue(QJsonValue::Null)}, {"id", r.id()}, {"providerLabel", r.providerLabel()}};
}
QJsonValue wire(const UsageAttributionBreakdown::Row &r) {
    return QJsonObject{{"state", wire(r.state)}, {"tokens", r.tokens}, {"cost", r.cost}, {"id", r.id()}};
}
QJsonValue wire(const AttributedDailySeries::Point &p) {
    return QJsonObject{{"date", p.date}, {"state", wire(p.state)}, {"model", p.model}, {"tokens", p.tokens}, {"cost", p.cost}};
}
QJsonValue wire(const WindowUsage::SubscriptionTotal &t) {
    return QJsonObject{{"target", t.target}, {"tokens", t.tokens}, {"cost", t.cost}};
}
QJsonValue wire(qint64 v) { return v; }
template<class T> QJsonArray wire(const QList<T> &values) {
    QJsonArray out;
    for (const auto &v : values) out.append(wire(v));
    return out;
}
QJsonValue wire(const SubscriptionTrend &t) {
    QJsonArray days;
    for (const auto &d : t.days) {
        QJsonObject buckets;
        for (auto it = d.byTarget.constBegin(); it != d.byTarget.constEnd(); ++it)
            buckets[it.key()] = QJsonObject{{"tokens", it->tokens}, {"cost", it->cost}};
        days.append(QJsonObject{{"date", d.date}, {"byTarget", buckets}, {"totalTokens", d.totalTokens}, {"totalCost", d.totalCost}, {"isEmpty", d.isEmpty()}});
    }
    return QJsonObject{{"days", days}, {"targets", QJsonArray::fromStringList(t.targets)}, {"targetsByTokens", QJsonArray::fromStringList(t.targetsByTokens)},
        {"costOrder", QJsonArray::fromStringList(t.targetsFor(false))}, {"tokenOrder", QJsonArray::fromStringList(t.targetsFor(true))},
        {"peakCost", t.peakCost}, {"peakTokens", t.peakTokens}};
}
QList<A::Record> records(const QJsonValue &v) {
    json::Decoder d{v, {}};
    if (!v.isArray()) d.fail("typeMismatch");
    const auto objects = v.toArray();
    QList<A::Record> out;
    for (const auto &o : objects) {
        json::Decoder r{o, {}};
        const auto state = r.field<QString>("state");
        A::State s;
        if (state == "assigned") s = {A::State::Kind::assigned, r.field<QString>("target")};
        else if (state == "excluded") s.kind = A::State::Kind::excluded;
        else if (state != "unassigned") throw std::runtime_error("invalid attribution-ext state");
        out.append({r.field<QString>("client"), r.field<QString>("provider"), r.field<std::optional<QString>>("model"), s});
    }
    return out;
}
S::RoutedSubscriptions routes(const QJsonValue &v) {
    json::Decoder d{v, {}};
    d.object();
    S::RoutedSubscriptions out;
    const auto o = v.toObject();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) out[it.key()] = json::Decoder{it.value(), {it.key()}}.decode<QStringList>();
    return out;
}
QJsonObject run(const QString &op, const QJsonObject &input) {
    auto field = [&]<class T>(const char *name) { return json::Decoder{input.value(name), {}}.decode<T>(); };
    auto optional = [](const std::optional<QString> &v) { return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null); };
    if (op == "copy") return {{"settings", QJsonArray::fromStringList(S::Copy::all())}, {"breakdown", QJsonArray::fromStringList(UsageAttributionBreakdown::Copy::all())}};
    if (op == "targets") {
        const auto payload = field.operator()<std::optional<Syrtis::AgentUsagePayload>>("payload");
        QJsonArray labels;
        for (const auto &label : field.operator()<QStringList>("labels")) labels.append(optional(S::subscriptionClient(label)));
        QJsonObject routed;
        const auto r = S::routedSubscriptions(payload);
        for (auto it = r.constBegin(); it != r.constEnd(); ++it) routed[it.key()] = QJsonArray::fromStringList(it.value());
        return {{"subscriptions", QJsonArray::fromStringList(S::subscriptionClients(payload))}, {"routed", routed}, {"labels", labels}};
    }
    if (op == "suggestion") {
        const auto state = S::suggestionTarget(field.operator()<QString>("sourceClient"), field.operator()<QString>("provider"), field.operator()<QStringList>("subscriptions"), routes(input["routed"]));
        return {{"state", state ? wire(*state) : QJsonValue(QJsonValue::Null)}};
    }
    if (op == "accept" || op == "writeFailure") {
        const auto updates = records(input["records"]);
        const auto confirmed = input.value("confirmedRaw"), suggestions = input.value("suggestionsRaw");
        if (op == "accept") {
            const auto result = S::accept(updates, confirmed, suggestions);
            return {{"confirmedRaw", result.confirmedRaw.isUndefined() ? QJsonValue(QJsonValue::Null) : result.confirmedRaw},
                {"suggestionsRaw", result.suggestionsRaw.isUndefined() ? QJsonValue(QJsonValue::Null) : result.suggestionsRaw}, {"failure", optional(result.failure)}};
        }
        const auto table = A::parseState(confirmed);
        const auto result = A::confirmedRaw(confirmed, updates);
        const auto failure = S::writeFailure(table, updates, result);
        return {{"result", optional(result)}, {"failure", failure ? QJsonValue(S::message(*failure)) : QJsonValue(QJsonValue::Null)}};
    }
    const auto confirmed = records(input["confirmed"]);
    if (op == "daily") {
        const auto points = AttributedDailySeries::points(field.operator()<QList<Contribution>>("contributions"), confirmed);
        const auto today = field.operator()<QString>("today");
        const auto count = field.operator()<qint64>("days");
        const auto range = SubscriptionTrendFold::calendarRange(today, count);
        return {{"points", wire(points)}, {"trend", wire(SubscriptionTrendFold::build(points, today, count))},
            {"range", range ? QJsonValue(QJsonArray::fromStringList(*range)) : QJsonValue(QJsonValue::Null)}};
    }
    if (op == "window") {
        const auto usage = field.operator()<WindowUsage>("usage");
        const auto totals = usage.totals(confirmed);
        QJsonArray messages;
        for (const auto &m : usage.messages) messages.append(QJsonObject{{"tokens", m.tokens()}, {"tokensExCacheRead", m.tokensExCacheRead()}});
        return {{"undatedCount", usage.undatedCount}, {"processingTimeMs", usage.processingTimeMs}, {"messages", messages},
            {"assigned", wire(totals.assigned)}, {"excluded", wire(totals.excluded)}, {"unassigned", wire(totals.unassigned)},
            {"buckets", wire(usage.buckets(field.operator()<qint64>("from"), field.operator()<qint64>("bucketMs"), field.operator()<qint64>("count")))}};
    }
    const auto entries = field.operator()<QList<ModelReportEntry>>("entries");
    if (op == "breakdown") return {{"rows", wire(UsageAttributionBreakdown::rows(entries, field.operator()<QStringList>("clients"), confirmed))}};
    const auto subscriptions = field.operator()<QStringList>("subscriptions");
    const auto routed = routes(input["routed"]);
    const auto rows = S::rows(entries, confirmed, records(input["suggestions"]));
    const auto proposed = S::suggestionRecords(entries, confirmed, subscriptions, routed);
    const auto summary = S::onboardingSummary(entries, confirmed, subscriptions, routed);
    const auto state = S::pageState(field.operator()<bool>("hasReport"), rows.size(), field.operator()<bool>("isLoading"));
    const QStringList states{"loading", "unavailable", "empty", "rows"};
    return {{"rows", wire(rows)}, {"proposed", wire(proposed)}, {"accepted", wire(S::acceptanceRecords(rows))},
        {"onboarding", QJsonObject{{"records", wire(summary.records)}, {"unsuggestedCount", summary.unsuggestedCount}}},
        {"pageState", states[int(state)]}, {"signature", S::signature(entries, subscriptions, field.operator()<bool>("targetsKnown"), routed)}};
}
}
QByteArray attributionExt(const QString &fixturesDir) {
    QFile file(QDir(fixturesDir).filePath("attribution-ext.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read attribution-ext fixture");
    const auto root = json::parse(file.readAll());
    if (!root.isObject() || !root.toObject()["cases"].isArray()) throw std::runtime_error("invalid attribution-ext cases");
    QSet<QString> ids;
    QJsonArray out;
    for (const auto &v : root.toObject()["cases"].toArray()) {
        const auto c = v.toObject();
        if (!c["id"].isString() || ids.contains(c["id"].toString()) || !c["input"].isObject()
            || !QStringList{"copy", "targets", "suggestion", "accept", "writeFailure", "daily", "window", "breakdown", "settings"}.contains(c["op"].toString()))
            throw std::runtime_error("invalid attribution-ext case");
        ids.insert(c["id"].toString());
        QJsonObject result{{"id", c["id"]}};
        try { result["value"] = run(c["op"].toString(), c["input"].toObject()); }
        catch (const DecodingError &e) {
            QJsonObject error{{"kind", e.kind}, {"path", QJsonArray::fromStringList(e.path)}};
            if (!e.field.isEmpty()) error["field"] = e.field;
            result["error"] = error;
        }
        out.append(result);
    }
    return QJsonDocument(QJsonObject{{"cases", out}}).toJson();
}
