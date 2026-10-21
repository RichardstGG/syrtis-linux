#pragma once
#include "ModelReport.h"

namespace syrtis::core::UsageAttribution {
inline const QString confirmedKey = "tokenbar.usage.attribution.confirmed";
inline const QString suggestionsKey = "tokenbar.usage.attribution.suggestions";
inline constexpr qsizetype maxEntries = 128, maxRawBytes = 64 * 1024;
struct State {
    enum class Kind { assigned, excluded, unassigned };
    Kind kind = Kind::unassigned;
    QString target;
    bool operator==(const State &other) const;
};
struct Record {
    QString client, provider;
    std::optional<QString> model;
    State state;
    bool operator==(const Record &other) const;
};
struct Table {
    QList<Record> records;
    bool isWritable = true;
    State state(const ModelReportEntry &entry) const;
    bool operator==(const Table &) const = default;
};
State resolve(const QString &client, const QString &provider, const std::optional<QString> &model,
              const QList<Record> &records);
State resolve(const ModelReportEntry &entry, const QList<Record> &records);
bool declares(const QString &subscription, const QList<Record> &records);
// Undefined means absent; every present non-string is foreign and read-only.
Table parseState(const QJsonValue &raw = QJsonValue::Undefined);
Table parseRaw(const std::optional<QString> &raw);
std::optional<QString> confirmedRaw(const QJsonValue &raw, const QList<Record> &updates);
std::optional<QString> suggestionsRaw(const QJsonValue &raw, const QList<Record> &updates);
std::optional<QString> replacingSuggestionsRaw(const QJsonValue &raw, const QList<Record> &records);
}
