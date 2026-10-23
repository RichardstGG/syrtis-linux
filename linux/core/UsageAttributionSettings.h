#pragma once
#include "UsageAttribution.h"
#include "AgentUsage.h"
#include <QtCore/QMap>
#include <QtCore/QSet>

namespace syrtis::core::UsageAttributionSettings {
namespace Copy {
extern const QString section;
extern const QString classifyHint;
extern const QString canonicalizationHint;
extern const QString noRows;
extern const QString unavailable;
extern const QString acceptSuggestions;
extern const QString suggestionsHint;
extern const QString source;
extern const QString observed;
extern const QString classification;
extern const QString unassigned;
extern const QString excluded;
extern const QString assigned;
extern const QString suggested;
extern const QString suggestedExcluded;
extern const QString unspecifiedProvider;
extern const QString classificationFor;
QStringList all();
}
enum class WriteFailure { invalidExistingValue, entryLimit, sizeOrInvalidRecord };
QString message(WriteFailure failure);
struct Row {
    QString client, provider;
    qint64 tokens;
    double cost;
    UsageAttribution::State state;
    std::optional<UsageAttribution::State> suggestedState;
    QString id() const;
    QString providerLabel() const;
    bool operator==(const Row &other) const;
};
using RoutedSubscriptions = QMap<QString, QStringList>;
extern const QSet<QString> crossAgentSubscriptionProviders, subscriptionBoundProviders, ownRowsOnlySubscriptions;
extern const QMap<QString, QString> providerOwnClient;
extern const QMap<QString, QSet<QString>> subscriptionProviderMap;
QStringList subscriptionClients(const std::optional<syrtis::core::AgentUsagePayload> &payload);
std::optional<QString> subscriptionClient(const QString &label);
RoutedSubscriptions routedSubscriptions(const std::optional<syrtis::core::AgentUsagePayload> &payload);
enum class PageState { loading, unavailable, empty, rows };
PageState pageState(bool hasReport, qsizetype rowCount, bool isLoading);
QList<Row> rows(const QList<ModelReportEntry> &entries, const QList<UsageAttribution::Record> &confirmed,
                const QList<UsageAttribution::Record> &suggestions);
std::optional<UsageAttribution::State> suggestionTarget(const QString &sourceClient, const QString &provider,
    const QStringList &subscriptions, const RoutedSubscriptions &routed = {});
QList<UsageAttribution::Record> suggestionRecords(const QList<ModelReportEntry> &entries,
    const QList<UsageAttribution::Record> &confirmed, const QStringList &subscriptions, const RoutedSubscriptions &routed = {});
QList<UsageAttribution::Record> acceptanceRecords(const QList<Row> &rows);
struct OnboardingSummary {
    QList<UsageAttribution::Record> records;
    qsizetype unsuggestedCount;
    bool operator==(const OnboardingSummary &) const = default;
};
OnboardingSummary onboardingSummary(const QList<ModelReportEntry> &entries,
    const QList<UsageAttribution::Record> &confirmed, const QStringList &subscriptions, const RoutedSubscriptions &routed = {});
std::optional<WriteFailure> writeFailure(const UsageAttribution::Table &table,
    const QList<UsageAttribution::Record> &updates, const std::optional<QString> &result);
std::optional<WriteFailure> writeFailure(const UsageAttribution::Table &table,
    const UsageAttribution::Record &record, const std::optional<QString> &result);
// Pure counterpart of accept(defaults:). The settings owner applies both strings
// only after success; failure and empty updates preserve both original values.
struct Acceptance {
    QJsonValue confirmedRaw, suggestionsRaw;
    std::optional<QString> failure;
};
Acceptance accept(const QList<UsageAttribution::Record> &records,
    const QJsonValue &confirmedRaw = QJsonValue::Undefined, const QJsonValue &suggestionsRaw = QJsonValue::Undefined);
QString signature(const QList<ModelReportEntry> &entries, const QStringList &subscriptions,
    bool targetsKnown = true, const RoutedSubscriptions &routed = {});
}
