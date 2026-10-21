#include "charts.h"
#include "DayBars.h"
#include "UsageStats.h"
#include "UsageTrace.h"
#include "OverviewCard.h"
#include "TBCore.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <algorithm>

using namespace syrtis::core;
namespace {
QJsonValue wire(const QString &v) { return v; }
QJsonValue wire(bool v) { return v; }
QJsonValue wire(qint64 v) { return v; }
QJsonValue wire(double v) { return v; }
QJsonValue wire(const TrayTotals &v);
QJsonValue wire(const TokenBreakdown &v);
QJsonValue wire(const ContributionClient &v);
QJsonValue wire(const ContributionTotals &v);
QJsonValue wire(const Contribution &v);
QJsonValue wire(const DateRange &v);
QJsonValue wire(const YearMeta &v);
QJsonValue wire(const UsageMeta &v);
QJsonValue wire(const UsageSummary &v);
QJsonValue wire(const UsagePayload &v);
QJsonValue wire(const TraceBucket &v);
QJsonValue wire(const TokensPerMin &v);
QJsonValue wire(const PerDay &v);
QJsonValue wire(const DaySegment &v);
QJsonValue wire(const DayBar &v);
template<class T> QJsonValue wire(const QList<T> &v);
template<class T> QJsonValue wire(const QHash<QString, T> &v);
template<class T> QJsonValue wire(const std::optional<T> &v) { return v ? wire(*v) : QJsonValue(QJsonValue::Null); }
template<class T> QJsonValue wire(const QList<T> &v) {
    QJsonArray out;
    for (const auto &item : v) out.append(wire(item));
    return out;
}
template<class T> QJsonValue wire(const QHash<QString, T> &v) {
    QJsonObject out;
    for (auto it = v.begin(); it != v.end(); ++it) out[it.key()] = wire(it.value());
    return out;
}
QJsonValue wire(const TrayTotals &v) {
    return QJsonObject{{"todayTokens", wire(v.todayTokens)}, {"todayCost", wire(v.todayCost)}, {"totalTokens", wire(v.totalTokens)}, {"totalCost", wire(v.totalCost)}, {"todayTopClient", wire(v.todayTopClient)}};
}
QJsonValue wire(const TokenBreakdown &v) {
    return QJsonObject{{"input", wire(v.input)}, {"output", wire(v.output)}, {"cacheRead", wire(v.cacheRead)}, {"cacheWrite", wire(v.cacheWrite)}, {"reasoning", wire(v.reasoning)}};
}
QJsonValue wire(const ContributionClient &v) {
    return QJsonObject{{"client", wire(v.client)}, {"modelId", wire(v.modelId)}, {"providerId", wire(v.providerId)}, {"tokens", wire(v.tokens)}, {"cost", wire(v.cost)}, {"messages", wire(v.messages)}};
}
QJsonValue wire(const ContributionTotals &v) {
    return QJsonObject{{"tokens", wire(v.tokens)}, {"cost", wire(v.cost)}, {"messages", wire(v.messages)}};
}
QJsonValue wire(const Contribution &v) {
    return QJsonObject{{"date", wire(v.date)}, {"totals", wire(v.totals)}, {"intensity", wire(v.intensity)}, {"tokenBreakdown", wire(v.tokenBreakdown)}, {"clients", wire(v.clients)}, {"turnsByClient", wire(v.turnsByClient)}};
}
QJsonValue wire(const DateRange &v) {
    return QJsonObject{{"start", wire(v.start)}, {"end", wire(v.end)}};
}
QJsonValue wire(const YearMeta &v) {
    return QJsonObject{{"year", wire(v.year)}, {"totalTokens", wire(v.totalTokens)}, {"totalCost", wire(v.totalCost)}, {"range", wire(v.range)}};
}
QJsonValue wire(const UsageMeta &v) {
    return QJsonObject{{"generatedAt", wire(v.generatedAt)}, {"version", wire(v.version)}, {"dateRange", wire(v.dateRange)}};
}
QJsonValue wire(const UsageSummary &v) {
    return QJsonObject{{"totalTokens", wire(v.totalTokens)}, {"totalCost", wire(v.totalCost)}, {"totalDays", wire(v.totalDays)}, {"activeDays", wire(v.activeDays)}, {"averagePerDay", wire(v.averagePerDay)}, {"maxCostInSingleDay", wire(v.maxCostInSingleDay)}, {"clients", wire(v.clients)}, {"models", wire(v.models)}};
}
QJsonValue wire(const UsagePayload &v) {
    return QJsonObject{{"meta", wire(v.meta)}, {"summary", wire(v.summary)}, {"years", wire(v.years)}, {"contributions", wire(v.contributions)}};
}
QJsonValue wire(const TraceBucket &v) {
    return QJsonObject{{"client", wire(v.client)}, {"agent", wire(v.agent)}, {"model", wire(v.model)}, {"tokens", wire(v.tokens)}, {"messages", wire(v.messages)}, {"tokensPerMin", wire(v.tokensPerMin)}};
}
QJsonValue wire(const TokensPerMin &v) {
    return QJsonObject{{"tokensPerMin", wire(v.tokensPerMin)}};
}
QJsonValue wire(const PerDay &v) {
    return QJsonObject{{"date", v.date}, {"tokens", v.tokens}, {"cost", v.cost},
        {"intensity", v.intensity}, {"hasMessages", v.hasMessages}, {"isActive", v.isActive()}};
}
QJsonValue wire(const UsageStats &v) {
    return QJsonObject{{"totalTokens", v.totalTokens}, {"totalCost", v.totalCost}, {"activeDays", v.activeDays},
        {"bestDay", v.bestDay ? QJsonValue(QJsonObject{{"date", v.bestDay->date}, {"cost", v.bestDay->cost}}) : QJsonValue(QJsonValue::Null)},
        {"averagePerDay", v.averagePerDay}, {"dateRange", wire(v.dateRange)}, {"perDay", wire(v.perDay)},
        {"perDayMap", wire(v.perDayMap)}, {"streaks", QJsonObject{{"longest", v.streaks.longest}, {"current", v.streaks.current}}},
        {"presentClients", wire(v.presentClients)}, {"maxTokens", v.maxTokens}};
}
QJsonValue wire(const DaySegment &v) {
    return QJsonObject{{"key", v.key}, {"label", v.label}, {"color", v.color}, {"tokens", v.tokens}, {"cost", v.cost}};
}
QJsonValue wire(const DayBar &v) {
    return QJsonObject{{"date", v.date}, {"segments", wire(v.segments)}, {"totalTokens", v.totalTokens()},
                       {"totalCost", v.totalCost()}, {"isEmpty", v.isEmpty()}};
}
QStringList strings(const QJsonObject &c, const QString &key) {
    return json::Decoder{c, {}}.field<QList<QString>>(key);
}
QSet<QString> ids(const QJsonObject &c, const QString &key) {
    const auto list = strings(c, key);
    return QSet<QString>(list.begin(), list.end());
}
QStringList sorted(const QSet<QString> &set) {
    auto list = set.values(); std::sort(list.begin(), list.end()); return list;
}
QList<ModelColorEntry> entries(const QJsonObject &c) {
    QList<ModelColorEntry> out;
    for (const auto &v : c["entries"].toArray()) {
        const auto e = v.toObject();
        double cost;
        if (e["cost"] == QJsonValue("nan")) cost = std::numeric_limits<double>::quiet_NaN();
        else if (e["cost"] == QJsonValue("infinity")) cost = std::numeric_limits<double>::infinity();
        else cost = json::Decoder{e, {}}.field<double>("cost");
        out.append({json::Decoder{e, {}}.field<QString>("provider"), json::Decoder{e, {}}.field<QString>("model"), cost});
    }
    return out;
}
QJsonValue legend(const QList<DayBar> &bars, ChartMetric metric) {
    auto rows = DayBars::legend(bars, metric);
    // Equal-weight dictionary ordering is unspecified in Swift. Canonicalize
    // ties only at serialization; retain and test the production metric order.
    const auto sameWeight = [metric](const DaySegment &a, const DaySegment &b) {
        return metric == ChartMetric::cost ? a.cost == b.cost : a.tokens == b.tokens;
    };
    for (auto first = rows.begin(); first != rows.end();) {
        auto last = first + 1;
        while (last != rows.end() && sameWeight(*last, *first)) ++last;
        std::sort(first, last, [](const auto &a, const auto &b) { return a.key < b.key; });
        first = last;
    }
    return wire(rows);
}
template<class T> QJsonObject decode(const QJsonObject &c) {
    const auto input = c.contains("rawJSON") ? json::parse(c["rawJSON"].toString().toUtf8()) : c["input"];
    const auto value = TBCore::decode<T>(input);
    QJsonObject out{{"value", wire(value)}};
    if constexpr (std::is_same_v<T, TokenBreakdown>) out["total"] = value.total();
    return out;
}
QJsonObject run(const QJsonObject &c) {
    const auto op = c["op"].toString();
    const json::Decoder d{c, {}};
    if (op == "payload") {
        const auto payload = TBCore::decode<UsagePayload>(c["input"]);
        const auto selected = ids(c, "selected"), hidden = ids(c, "hidden");
        const auto onlyList = d.field<std::optional<QList<QString>>>("only");
        const auto only = onlyList ? std::optional<QSet<QString>>(QSet<QString>(onlyList->begin(), onlyList->end())) : std::nullopt;
        const UsageStats stats(payload, selected);
        const ModelColorMap colors(entries(c));
        QJsonObject out{{"value", wire(payload)}, {"stats", wire(stats)},
            {"tray", wire(payload.trayTotals(hidden, d.field<QString>("today"), only))},
            {"visibleYears", wire(sorted(UsageStats::yearsWithVisibleActivity(payload.contributions, hidden)))},
            {"hasVisibleActivity", UsageStats::hasVisibleActivity(payload.contributions, hidden)}};
        QJsonArray turns;
        for (const auto &contribution : payload.contributions)
            turns.append(QJsonObject{{"selected", wire(contribution.turns(strings(c, "selected")))},
                {"empty", wire(contribution.turns({}))}});
        out["turns"] = turns;
        for (const auto &[name, mode] : QList<std::pair<QString, StackBy>>{{"model", StackBy::model}, {"agent", StackBy::agent}}) {
            const auto bars = DayBars::build(payload, strings(c, "selected"), mode, colors,
                d.field<QString>("rangeEnd"), d.field<QString>("endFallback"));
            out[name] = QJsonObject{{"bars", wire(bars)}, {"tokenLegend", legend(bars, ChartMetric::tokens)},
                {"costLegend", legend(bars, ChartMetric::cost)}};
        }
        return out;
    }
    if (op == "colors") {
        const ModelColorMap colors(entries(c));
        const ModelColorMap reportColors(d.field<std::optional<ModelReport>>("report"));
        QJsonArray lookups, shades;
        for (const auto &v : c["lookups"].toArray()) {
            const json::Decoder lookup{v, {}};
            const auto provider = lookup.field<std::optional<QString>>("provider");
            const auto model = lookup.field<QString>("model");
            lookups.append(QJsonObject{{"inferred", ModelColors::providerFromModel(model)},
                {"key", ModelColors::providerColorKey(provider, model)}, {"color", colors.color(provider, model)},
                {"reportColor", reportColors.color(provider, model)}});
        }
        for (const auto &v : c["shades"].toArray()) {
            const json::Decoder shade{v, {}};
            shades.append(ModelColors::shadeFromBase(shade.field<QString>("hex"), shade.field<qint64>("rank")));
        }
        return {{"providerBase", wire(ModelColors::providerBase)}, {"lookups", lookups}, {"shades", shades}};
    }
    if (op == "trace") {
        const auto buckets = TBCore::decode<QList<TraceBucket>>(c["input"]);
        return {{"value", wire(buckets)}, {"totalRate", TraceBucket::totalRate(buckets, ids(c, "hidden"))},
                {"collapsed", wire(TraceBucket::collapseByClient(buckets))}};
    }
    if (op == "overview") {
        QJsonObject labels;
        for (const auto &id : OverviewCard::allCases()) labels[id] = OverviewCard::label(id);
        return {{"allCases", wire(OverviewCard::allCases())}, {"toggleable", wire(OverviewCard::toggleable())},
            {"hiddenKey", OverviewCard::hiddenKey}, {"labels", labels},
            {"visible", wire(OverviewCard::visible(d.field<QString>("hiddenRaw")))}};
    }
    if (op == "streaks") {
        QHash<QString, PerDay> days;
        for (const auto &v : c["days"].toArray()) {
            const json::Decoder day{v, {}};
            const auto date = day.field<QString>("date");
            days[date] = {date, day.field<qint64>("tokens"), day.field<double>("cost"), 0, day.field<bool>("hasMessages")};
        }
        const auto streaks = Streaks::compute(days, d.field<QString>("start"), d.field<QString>("end"));
        return {{"longest", streaks.longest}, {"current", streaks.current}};
    }
    if (op == "decode") {
        const auto type = c["type"].toString();
#define DECODE(T) if (type == #T) return decode<T>(c)
        DECODE(TokenBreakdown); DECODE(ContributionClient); DECODE(Contribution); DECODE(DateRange);
        DECODE(YearMeta); DECODE(UsagePayload); DECODE(TraceBucket); DECODE(TokensPerMin);
#undef DECODE
    }
    throw std::runtime_error("unknown charts fixture operation or type");
}
}
QByteArray charts(const QString &fixturesDir)
{
    QFile file(QDir(fixturesDir).filePath("charts.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read charts fixture");
    const auto root = json::parse(file.readAll());
    if (!root.isObject() || !root.toObject()["cases"].isArray()) throw std::runtime_error("invalid charts fixture cases");
    QJsonArray out;
    for (const auto &v : root.toObject()["cases"].toArray()) {
        if (!v.isObject() || !v.toObject()["id"].isString() || !v.toObject()["op"].isString())
            throw std::runtime_error("invalid charts fixture case");
        const auto c = v.toObject();
        QJsonObject result;
        try { result = run(c); }
        catch (const DecodingError &e) {
            QJsonObject error{{"kind", e.kind}, {"path", wire(e.path)}};
            if (e.kind == "keyNotFound") error["field"] = e.field;
            result["error"] = error;
        }
        result["id"] = c["id"];
        out.append(result);
    }
    return QJsonDocument(QJsonObject{{"cases", out}}).toJson();
}
