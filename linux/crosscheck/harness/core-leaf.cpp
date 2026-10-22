#include "core-leaf.h"
#include "ClientRegistry.h"
#include "Grid.h"
#include "ModelGrouping.h"
#include "ModelScope.h"
#include "WindowResolution.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <stdexcept>

using namespace syrtis::core;
namespace R = syrtis::core::ClientRegistry;
namespace {
QStringList list(const QJsonValue &value)
{
    QStringList out;
    for (const auto &entry : value.toArray()) out.append(entry.toString());
    return out;
}
QSet<QString> set(const QJsonValue &value)
{
    const auto ids = list(value);
    return {ids.cbegin(), ids.cend()};
}
QJsonArray array(const QStringList &ids) { return QJsonArray::fromStringList(ids); }
QJsonArray array(const QSet<QString> &ids)
{
    auto sorted = ids.values();
    sorted.sort();
    return array(sorted);
}
std::optional<qint64> integer(const QJsonValue &value)
{
    return value.isDouble() ? std::optional<qint64>(value.toInteger()) : std::nullopt;
}
QJsonValue json(std::optional<qint64> value) { return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null); }
QJsonValue json(const std::optional<QString> &value) { return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null); }
QJsonObject day(const ISODay &value)
{
    return {{"number", value.number}, {"iso", value.iso()}, {"weekday", value.weekday()}};
}
QJsonObject registryCase(const QJsonObject &c)
{
    const auto present = list(c["present"]), quotas = list(c["quotaIds"]);
    const auto hidden = set(c["tabHidden"]), limits = set(c["limitsHidden"]);
    const auto raw = c["raw"].toString(), order = c["orderRaw"].toString();
    const auto prefs = c["preferences"].toObject();
    ClientPreferences p;
    if (prefs["order"].isString()) p.order = prefs["order"].toString();
    if (prefs["legacyOrder"].isString()) p.legacyOrder = prefs["legacyOrder"].toString();
    p.hidden = prefs["hidden"].toString();
    p.limitsHidden = prefs["limitsHidden"].toString();
    QJsonObject out{
        {"parseIdList", array(R::parseIdList(raw))}, {"parseIdSet", array(R::parseIdSet(raw))},
        {"tabClients", array(R::tabClients(present, quotas))},
        {"quotaClients", array(R::quotaClients(present, quotas, hidden, order))},
        {"withGroupMembers", array(R::withGroupMembers(hidden))},
        {"hiddenTabClients", array(R::hiddenTabClients(hidden))},
        {"quotaExcludedClients", array(R::quotaExcludedClients(hidden, limits))},
        {"knownLimitsClients", array(R::knownLimitsClients(present, quotas, set(c["placeholders"])))},
        {"orderedClients", array(R::orderedClients(present, order))},
        {"displayClients", array(R::displayClients(present, c["hiddenRaw"].toString(), order))},
        {"reorder", array(R::reorder(list(c["list"]), c["fromId"].toString(), c["toId"].toString()))},
        {"mergeReorder", array(R::mergeReorder(list(c["full"]), list(c["visible"]), c["fromId"].toString(), c["toId"].toString()))},
        {"settings", QJsonObject{
            {"hiddenClients", array(R::hiddenClients(p))},
            {"hiddenLimitsClients", array(R::hiddenLimitsClients(p))},
            {"hiddenTabClients", array(R::hiddenTabClients(p))},
            {"quotaExcludedClients", array(R::quotaExcludedClients(p))},
            {"orderedClients", array(R::orderedClients(present, p))},
            {"displayClients", array(R::displayClients(present, p))}}}
    };
    R::migrateLegacyOrderKey(p);
    out["migratedOrder"] = json(p.order);
    R::migrateLegacyOrderKey(p);
    out["migratedOrderAgain"] = json(p.order);
    return out;
}
}
QByteArray coreLeaf(const QString &fixturesDir)
{
    QFile file(QDir(fixturesDir).filePath(QStringLiteral("core-leaf.json")));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read core-leaf fixture");
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        throw std::runtime_error("invalid core-leaf fixture object");
    const auto root = doc.object();
    for (const auto *key : {"ids", "labels", "registry", "grouping", "scopes", "windows", "grids", "days", "dayNumbers"})
        if (!root[key].isArray()) throw std::runtime_error("missing core-leaf fixture array");
    QJsonObject out;
    out["allIds"] = array(R::allIds());
    out["keys"] = QJsonObject{{"order", R::tabOrderKey}, {"hidden", R::tabHiddenKey}, {"limitsHidden", R::limitsHiddenKey}};
    QJsonObject aliases;
    for (auto it = R::subscriptionLabelAliases.cbegin(); it != R::subscriptionLabelAliases.cend(); ++it) aliases[it.key()] = it.value();
    out["subscriptionLabelAliases"] = aliases;
    QJsonArray styles;
    for (const auto &id : R::allIds() + list(root["ids"])) {
        const auto s = R::style(id);
        styles.append(QJsonObject{{"id", s.id}, {"displayName", s.displayName}, {"color", s.color},
            {"shortName", R::shortName(id)}, {"canonicalClient", R::canonicalClient(id)},
            {"quotaOwner", R::quotaOwner(id)}, {"tabSlice", array(R::tabSlice(id))},
            {"tabLabel", R::tabLabel(id)}, {"tabDisplayName", R::tabDisplayName(id)}});
    }
    out["styles"] = styles;
    QJsonArray labels, registry, grouping, scopes, windows, grids, days, numbers;
    for (const auto &label : list(root["labels"])) labels.append(R::clientIdForSubscriptionLabel(label));
    for (const auto &c : root["registry"].toArray()) registry.append(registryCase(c.toObject()));
    for (const auto &id : list(root["grouping"])) grouping.append(ModelGrouping::groupID(id));
    for (const auto &value : root["scopes"].toArray()) {
        const auto c = value.toObject();
        scopes.append(ModelScope::covers(c["scope"].toString(), c["model"].toString()));
    }
    for (const auto &value : root["windows"].toArray()) {
        const auto c = value.toObject();
        QList<WindowMessage> messages;
        for (const auto &m : c["messages"].toArray()) {
            WindowMessage message;
            message.timestamp = m.toObject()["timestamp"].toInteger();
            messages.append(message);
        }
        const auto r = WindowResolver::resolve(integer(c["reset"]), integer(c["duration"]), c["now"].toInteger(), integer(c["first"]));
        QJsonObject result{{"firstUsageAfterReset", json(WindowResolver::firstUsageAfterReset(messages, c["probeReset"].toInteger()))}};
        switch (r.kind) {
        case WindowResolution::Kind::active: result["kind"] = "active"; break;
        case WindowResolution::Kind::idle: result["kind"] = "idle"; break;
        case WindowResolution::Kind::inferred: result["kind"] = "inferred"; break;
        case WindowResolution::Kind::unavailable: result["kind"] = "unavailable"; break;
        }
        if (r.kind == WindowResolution::Kind::active || r.kind == WindowResolution::Kind::inferred) {
            result["start"] = r.start;
            result["end"] = r.end;
        }
        windows.append(result);
    }
    for (const auto &value : root["grids"].toArray()) {
        const auto c = value.toObject();
        QHash<QString, PerDay> perDay;
        QJsonArray activity;
        for (const auto &v : c["entries"].toArray()) {
            const auto e = v.toObject();
            const PerDay entry{e["date"].toString(), e["tokens"].toInteger(), e["cost"].toDouble(), int(e["intensity"].toInteger()), e["hasMessages"].toBool()};
            perDay.insert(entry.date, entry);
            activity.append(entry.isActive());
        }
        const auto g = buildGrid(c["year"].toString(), perDay);
        QJsonArray cells;
        for (const auto &cell : g.cells)
            cells.append(QJsonObject{{"col", cell.col}, {"row", cell.row}, {"date", cell.date},
                {"inYear", cell.inYear}, {"active", cell.active}, {"tokens", cell.tokens}, {"cost", cell.cost}});
        grids.append(QJsonObject{{"cols", g.cols}, {"rows", g.rows}, {"maxTokens", g.maxTokens}, {"cells", cells}, {"perDayIsActive", activity}});
    }
    for (const auto &iso : list(root["days"])) {
        const auto d = ISODay::parse(iso);
        days.append(d ? QJsonValue(day(*d)) : QJsonValue(QJsonValue::Null));
    }
    for (const auto &n : root["dayNumbers"].toArray()) numbers.append(day(ISODay{n.toInteger()}));
    out["labels"] = labels; out["registry"] = registry; out["grouping"] = grouping; out["scopes"] = scopes;
    out["windows"] = windows; out["grids"] = grids; out["days"] = days; out["dayNumbers"] = numbers;
    return QJsonDocument(out).toJson();
}
