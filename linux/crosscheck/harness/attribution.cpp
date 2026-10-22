#include "attribution.h"
#include "QuotaHistory.h"
#include "WindowEquivalence.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSet>

using namespace syrtis::core;
namespace A = UsageAttribution;
namespace H = QuotaHistoryFold;
namespace E = WindowEquivalence;
namespace G = WindowCardGeometry;
namespace {
QJsonValue wire(const A::State &s) {
    QJsonObject out{{"kind", s.kind == A::State::Kind::assigned ? "assigned" : s.kind == A::State::Kind::excluded ? "excluded" : "unassigned"}};
    if (s.kind == A::State::Kind::assigned) out["target"] = s.target;
    return out;
}
QJsonValue wire(const A::Record &r) {
    return QJsonObject{{"client", r.client}, {"provider", r.provider}, {"model", r.model ? QJsonValue(*r.model) : QJsonValue(QJsonValue::Null)}, {"state", wire(r.state)}};
}
QJsonValue wire(const TokenBreakdown &b) {
    return QJsonObject{{"input", b.input}, {"output", b.output}, {"cacheRead", b.cacheRead}, {"cacheWrite", b.cacheWrite}, {"reasoning", b.reasoning}, {"total", b.total()}};
}
QJsonValue wire(const QuotaCycle &c) {
    return QJsonObject{{"resetAtMs", c.resetAtMs}, {"startMs", c.startMs}, {"usedPercent", c.usedPercent},
        {"risingRuns", c.risingRuns}, {"peakUsedPercent", c.peakUsedPercent}, {"sampleCount", c.sampleCount},
        {"firstSampleMs", c.firstSampleMs}, {"lastSampleMs", c.lastSampleMs}, {"observedFraction", c.observedFraction},
        {"evidenceStartMs", c.evidenceStartMs()}, {"durationMs", c.durationMs()}};
}
QJsonValue wire(const QuotaHistoryModel &m) {
    return QJsonObject{{"providerId", m.providerId}, {"modelId", m.modelId}, {"tokens", m.tokens}, {"breakdown", wire(m.breakdown)}, {"cost", m.cost}, {"id", m.id()}};
}
QJsonValue wire(const QuotaHistoryRow &r);
QJsonValue wire(const H::Span &s) { return QJsonObject{{"tokens", s.tokens}, {"cost", s.cost}}; }
QJsonValue wire(const QuotaSample &s) { return QJsonObject{{"atMs", s.atMs}, {"usedPercent", s.usedPercent}}; }
QJsonValue wire(const BarRect &r) { return QJsonObject{{"x", r.x}, {"width", r.width}, {"height", r.height}, {"isEmpty", r.isEmpty}}; }
QJsonValue wire(const CurvePoint &p) { return QJsonObject{{"x", p.x}, {"y", p.y}}; }
QJsonValue wire(const HitZone &z);
template<class T> QJsonValue wire(const QList<T> &v);
template<class T> QJsonValue wire(const std::optional<T> &v) { return v ? wire(*v) : QJsonValue(QJsonValue::Null); }
QJsonValue wire(double v) { return v; }
QJsonValue wire(const std::optional<QString> &v) { return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null); }
template<class T> QJsonValue wire(const QList<T> &v) {
    QJsonArray out;
    for (const auto &item : v) out.append(wire(item));
    return out;
}
QJsonValue wire(const HitZone &z) {
    const auto used = z.consumed(QuotaMetric::used), remaining = z.consumed(QuotaMetric::remaining);
    return QJsonObject{{"index", z.index}, {"loMs", z.loMs}, {"hiMs", z.hiMs}, {"x", z.x}, {"width", z.width},
        {"closingSample", wire(z.closingSample)}, {"openingSample", wire(z.openingSample)},
        {"consumedUsed", used ? QJsonValue(*used) : QJsonValue(QJsonValue::Null)},
        {"consumedRemaining", remaining ? QJsonValue(*remaining) : QJsonValue(QJsonValue::Null)}};
}
QJsonValue wire(const QuotaHistoryRow &r) {
    return QJsonObject{{"cycle", wire(r.cycle)}, {"mineTokens", r.mineTokens}, {"mineTokensExCacheRead", r.mineTokensExCacheRead},
        {"mineBreakdown", wire(r.mineBreakdown)}, {"mineCost", r.mineCost}, {"spanTokens", r.spanTokens}, {"spanCost", r.spanCost},
        {"otherTokens", r.otherTokens}, {"otherCost", r.otherCost}, {"otherHasAssigned", r.otherHasAssigned},
        {"otherHasExcluded", r.otherHasExcluded}, {"otherHasUnattributed", r.otherHasUnattributed}, {"models", wire(r.models)}, {"id", r.id()}};
}
QJsonValue wire(const E::Row &r) {
    QJsonObject out{{"isRatio", r.isRatio()}};
    switch (r.kind) {
    case E::Row::Kind::ratio: out["kind"] = "ratio"; out["tokensPerTenth"] = r.tokensPerTenth; out["costPerTenth"] = r.costPerTenth; out["errorPercent"] = r.errorPercent; break;
    case E::Row::Kind::insufficient: out["kind"] = "insufficient"; out["deltaPercent"] = r.deltaPercent; out["errorPercent"] = r.errorPercent; break;
    case E::Row::Kind::notMoved: out["kind"] = "notMoved"; break;
    case E::Row::Kind::unaccounted: out["kind"] = "unaccounted"; out["deltaPercent"] = r.deltaPercent; break;
    case E::Row::Kind::unavailable: out["kind"] = "unavailable"; break;
    case E::Row::Kind::costOnly: out["kind"] = "costOnly"; out["costPerTenth"] = r.costPerTenth; out["errorPercent"] = r.errorPercent; break;
    case E::Row::Kind::tokensOnly: out["kind"] = "tokensOnly"; out["tokensPerTenth"] = r.tokensPerTenth; out["errorPercent"] = r.errorPercent; break;
    case E::Row::Kind::tooFewCycles: out["kind"] = "tooFewCycles"; out["count"] = r.count; out["needed"] = r.needed; break;
    case E::Row::Kind::undeclared: out["kind"] = "undeclared"; break;
    case E::Row::Kind::spread: out["kind"] = "spread"; out["lowPerTenth"] = r.lowPerTenth; out["highPerTenth"] = r.highPerTenth; out["lowCostPerTenth"] = r.lowCostPerTenth; out["highCostPerTenth"] = r.highCostPerTenth; break;
    }
    out["text"] = E::text(r, [](qint64 n) { return QString::number(n); }, [](double n) { return QString::number(n, 'f', 3); });
    return out;
}
// Fixture declarations include unassigned updates, which the persisted codec
// intentionally refuses. Only construction belongs here; resolution is shipping code.
QList<A::Record> readRecords(const QJsonValue &value) {
    if (!value.isArray()) throw std::runtime_error("invalid attribution records");
    QList<A::Record> out;
    for (const auto &v : value.toArray()) {
        const json::Decoder d{v, {}};
        A::Record r{d.field<QString>("client"), d.field<QString>("provider"), d.field<std::optional<QString>>("model"), {}};
        const auto state = d.field<QString>("state");
        if (state == "assigned") r.state = {A::State::Kind::assigned, d.field<QString>("target")};
        else if (state == "excluded") r.state.kind = A::State::Kind::excluded;
        else if (state != "unassigned") throw std::runtime_error("invalid attribution state");
        out.append(r);
    }
    return out;
}
QList<QuotaSample> samples(const QJsonValue &v) {
    if (!v.isArray()) throw std::runtime_error("invalid attribution samples");
    QList<QuotaSample> out;
    for (const auto &s : v.toArray()) {
        const json::Decoder d{s, {}};
        out.append({d.field<qint64>("atMs"), d.field<double>("usedPercent")});
    }
    return out;
}
QJsonObject run(const QJsonObject &c) {
    const auto op = c["op"].toString();
    const auto input = c["input"].toObject();
    const json::Decoder d{c["input"], {}};
    if (op == "codec") {
        const auto table = A::parseState(input.value("raw"));
        const auto declarations = readRecords(d.value.toObject()["records"]);
        QJsonArray states;
        if (!input["queries"].isArray()) throw std::runtime_error("invalid attribution queries");
        for (const auto &v : input["queries"].toArray()) {
            const json::Decoder q{v, {}};
            states.append(wire(A::resolve(q.field<QString>("client"), q.field<QString>("provider"), q.field<std::optional<QString>>("model"), table.records)));
        }
        return {{"records", wire(table.records)}, {"isWritable", table.isWritable}, {"states", states},
            {"declares", A::declares(d.field<QString>("subscription"), table.records)},
            {"updatedRaw", wire(A::confirmedRaw(input.value("raw"), declarations))},
            {"suggestedRaw", wire(A::suggestionsRaw(input.value("raw"), declarations))},
            {"replacedRaw", wire(A::replacingSuggestionsRaw(input.value("raw"), declarations))}};
    }
    if (op == "aggregate") {
        if (!input["cycles"].isArray()) throw std::runtime_error("invalid attribution cycles");
        QList<E::Cycle> cycles;
        for (const auto &v : input["cycles"].toArray()) {
            const json::Decoder cd{v, {}};
            cycles.append({cd.field<double>("deltaPercent"), cd.field<qint64>("spanTokens"), cd.field<double>("spanCost"), cd.field<double>("observedFraction"), int(cd.field<qint64>("risingRuns"))});
        }
        return {{"row", wire(E::aggregate(cycles, d.field<bool>("declared")))}};
    }
    if (op == "readings") {
        const auto readings = d.field<QList<double>>("readings");
        return {{"consumed", H::consumed(readings)}, {"risingRuns", H::risingRuns(readings)},
            {"deltaQualifies", E::deltaQualifies(H::consumed(readings), H::risingRuns(readings))},
            {"lowerBound", H::lowerBound(d.field<QList<qint64>>("stamps"), d.field<qint64>("probe"))}};
    }
    const auto messages = json::Decoder{input["messages"], {}}.decode<QList<WindowMessage>>();
    if (op == "row") return {{"row", wire(E::row(samples(input["samples"]), messages))}};
    if (op == "geometry") {
        const auto metric = d.field<QString>("metric");
        if (metric != "used" && metric != "remaining") throw std::runtime_error("invalid attribution metric");
        const auto g = G::chartGeometry(d.field<qint64>("start"), d.field<qint64>("end"), d.field<qint64>("now"), samples(input["samples"]), messages,
            metric == "used" ? QuotaMetric::used : QuotaMetric::remaining);
        return {{"nowX", g.nowX}, {"firstSampleX", g.firstSampleX}, {"bars", wire(g.bars)}, {"hits", wire(g.hits)}, {"samplePoints", wire(g.samplePoints)}, {"curve", wire(g.curve)}};
    }
    if (op == "history") {
        const auto cycles = H::cycles(json::Decoder{input["points"], {}}.decode<QList<Syrtis::QuotaCurvePoint>>());
        const auto considered = H::considered(cycles);
        const auto subscription = d.field<QString>("subscription");
        const auto scope = d.field<std::optional<QString>>("modelScope");
        const auto confirmed = readRecords(input["records"]);
        const auto rows = H::rows(considered, messages, subscription, scope, confirmed);
        QList<E::Cycle> estimates;
        for (const auto &r : rows) estimates.append({r.cycle.usedPercent, r.spanTokens, r.spanCost, r.cycle.observedFraction, r.cycle.risingRuns});
        return {{"cycles", wire(cycles)}, {"considered", wire(considered)}, {"rows", wire(rows)},
            {"spans", wire(H::spans(considered, messages, subscription, scope, confirmed))},
            {"aggregate", wire(E::aggregate(subscription, confirmed, estimates))}};
    }
    throw std::runtime_error("unknown attribution operation");
}
}
QByteArray attribution(const QString &fixturesDir) {
    QFile file(QDir(fixturesDir).filePath("attribution.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read attribution fixture");
    const auto root = json::parse(file.readAll());
    if (!root.isObject() || !root.toObject()["cases"].isArray()) throw std::runtime_error("invalid attribution cases");
    QJsonArray out;
    QSet<QString> ids;
    for (const auto &v : root.toObject()["cases"].toArray()) {
        const auto c = v.toObject();
        if (!c["id"].isString() || ids.contains(c["id"].toString()) || !c["input"].isObject()
            || !QStringList{"codec", "aggregate", "readings", "row", "geometry", "history"}.contains(c["op"].toString()))
            throw std::runtime_error("invalid attribution case");
        ids.insert(c["id"].toString());
        QJsonObject result{{"id", c["id"]}};
        try { result["value"] = run(c); }
        catch (const DecodingError &e) {
            QJsonObject error{{"kind", e.kind}, {"path", QJsonArray::fromStringList(e.path)}};
            if (!e.field.isEmpty()) error["field"] = e.field;
            result["error"] = error;
        }
        out.append(result);
    }
    return QJsonDocument(QJsonObject{{"cases", out}}).toJson();
}
