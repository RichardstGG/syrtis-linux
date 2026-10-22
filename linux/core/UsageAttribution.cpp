#include "UsageAttribution.h"
#include "ClientRegistry.h"
#include <algorithm>
#include <set>
#include <tuple>

namespace syrtis::core::UsageAttribution {
namespace {
QString key(const QString &s) { return s.normalized(QString::NormalizationForm_C); }
bool same(const QString &a, const QString &b) { return key(a) == key(b); }
bool same(const std::optional<QString> &a, const std::optional<QString> &b) {
    return a && b ? same(*a, *b) : !a && !b;
}
auto sourceKey(const Record &r) {
    return std::tuple{key(r.client), key(r.provider), r.model ? std::optional<QString>(key(*r.model)) : std::nullopt};
}
bool persistable(const Record &r) {
    return !r.client.isEmpty() && (r.state.kind == State::Kind::excluded
        || (r.state.kind == State::Kind::assigned && ClientRegistry::allIds().contains(r.state.target)));
}
bool unique(const QList<Record> &records) {
    std::set<decltype(sourceKey(Record{}))> keys;
    for (const auto &r : records) if (!keys.insert(sourceKey(r)).second) return false;
    return true;
}
bool precedes(const Record &a, const Record &b) {
    // Swift String compares canonically equivalent strings by Unicode scalars.
    if (!same(a.client, b.client)) return key(a.client).toUcs4() < key(b.client).toUcs4();
    if (!same(a.provider, b.provider)) return key(a.provider).toUcs4() < key(b.provider).toUcs4();
    if (!a.model || !b.model) return !a.model && b.model.has_value();
    return key(*a.model).toUcs4() < key(*b.model).toUcs4();
}
std::optional<QString> canonicalRaw(QList<Record> records) {
    if (records.size() > maxEntries || !unique(records)
        || !std::all_of(records.begin(), records.end(), persistable)) return std::nullopt;
    std::stable_sort(records.begin(), records.end(), precedes);
    QJsonArray array;
    for (const auto &r : records) {
        QJsonObject object{{"client", r.client}, {"provider", r.provider},
            {"model", r.model ? QJsonValue(*r.model) : QJsonValue(QJsonValue::Null)},
            {"state", r.state.kind == State::Kind::assigned ? "assigned" : "excluded"}};
        if (r.state.kind == State::Kind::assigned) object["target"] = r.state.target;
        array.append(object);
    }
    // Foundation JSONSerialization escapes forward slashes by default.
    auto bytes = QJsonDocument(array).toJson(QJsonDocument::Compact);
    bytes.replace("/", "\\/");
    if (bytes.size() > maxRawBytes) return std::nullopt;
    return QString::fromUtf8(bytes);
}
}
bool State::operator==(const State &other) const {
    return kind == other.kind && (kind != Kind::assigned || same(target, other.target));
}
bool Record::operator==(const Record &other) const {
    return same(client, other.client) && same(provider, other.provider) && same(model, other.model) && state == other.state;
}
State resolve(const QString &client, const QString &provider, const std::optional<QString> &model,
              const QList<Record> &records) {
    if (model) for (const auto &r : records)
        if (same(r.client, client) && same(r.provider, provider) && r.model && same(*r.model, *model)) return r.state;
    for (const auto &r : records)
        if (same(r.client, client) && same(r.provider, provider) && !r.model) return r.state;
    return {};
}
State resolve(const ModelReportEntry &entry, const QList<Record> &records) {
    return resolve(entry.client, entry.provider, entry.model, records);
}
State Table::state(const ModelReportEntry &entry) const { return resolve(entry, records); }
bool declares(const QString &subscription, const QList<Record> &records) {
    return std::any_of(records.begin(), records.end(), [&](const auto &r) {
        return r.state.kind == State::Kind::assigned && same(r.state.target, subscription);
    });
}
Table parseRaw(const std::optional<QString> &raw) {
    return parseState(raw ? QJsonValue(*raw) : QJsonValue(QJsonValue::Undefined));
}
Table parseState(const QJsonValue &value) {
    if (value.isUndefined()) return {};
    if (!value.isString() || value.toString().toUtf8().size() > maxRawBytes) return {{}, false};
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size() > maxEntries) return {{}, false};
    QList<Record> records;
    for (const auto &v : doc.array()) {
        if (!v.isObject()) return {{}, false};
        const auto o = v.toObject();
        if (!o["client"].isString() || !o["provider"].isString() || !o["state"].isString()
            || !(o["model"].isNull() || o["model"].isString())) return {{}, false};
        Record r{o["client"].toString(), o["provider"].toString(),
            o["model"].isString() ? std::optional<QString>(o["model"].toString()) : std::nullopt, {}};
        auto expected = QStringList{"client", "model", "provider", "state"};
        if (o["state"] == "assigned") {
            if (!o["target"].isString()) return {{}, false};
            expected.append("target");
            r.state = {State::Kind::assigned, o["target"].toString()};
        } else if (o["state"] == "excluded") r.state.kind = State::Kind::excluded;
        else return {{}, false};
        if (o.keys() != expected || !persistable(r)) return {{}, false};
        records.append(r);
    }
    if (!unique(records)) return {{}, false};
    std::stable_sort(records.begin(), records.end(), precedes);
    return {records, true};
}
std::optional<QString> confirmedRaw(const QJsonValue &raw, const QList<Record> &updates) {
    auto parsed = parseState(raw);
    if (!parsed.isWritable) return std::nullopt;
    for (const auto &r : updates) {
        if (r.client.isEmpty() || (r.state.kind != State::Kind::unassigned && !persistable(r))) return std::nullopt;
        parsed.records.removeIf([&](const auto &old) { return sourceKey(old) == sourceKey(r); });
        if (r.state.kind != State::Kind::unassigned) parsed.records.append(r);
    }
    return canonicalRaw(parsed.records);
}
std::optional<QString> suggestionsRaw(const QJsonValue &raw, const QList<Record> &updates) {
    return confirmedRaw(raw, updates);
}
std::optional<QString> replacingSuggestionsRaw(const QJsonValue &raw, const QList<Record> &records) {
    return parseState(raw).isWritable ? canonicalRaw(records) : std::nullopt;
}
}
