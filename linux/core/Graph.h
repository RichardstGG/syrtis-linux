#pragma once
#include "JsonDecoder.h"
#include "TokenBreakdown.h"
#include <QtCore/QHash>
#include <QtCore/QSet>

namespace syrtis::core {
struct TrayTotals {
    qint64 todayTokens;
    double todayCost;
    qint64 totalTokens;
    double totalCost;
    std::optional<QString> todayTopClient;
};
struct ContributionClient {
    QString client;
    QString modelId;
    QString providerId;
    TokenBreakdown tokens;
    double cost;
    qint64 messages;
    static ContributionClient decode(const json::Decoder &decoder);
};
struct ContributionTotals {
    qint64 tokens;
    double cost;
    qint64 messages;
    static ContributionTotals decode(const json::Decoder &decoder);
};
struct Contribution {
    using Totals = ContributionTotals;
    QString date;
    ContributionTotals totals;
    qint64 intensity;
    TokenBreakdown tokenBreakdown;
    QList<ContributionClient> clients;
    std::optional<QHash<QString, qint64>> turnsByClient;
    std::optional<qint64> turns(const QStringList &clients) const;
    static Contribution decode(const json::Decoder &decoder);
};
struct DateRange {
    QString start;
    QString end;
    static DateRange decode(const json::Decoder &decoder);
};
struct YearMeta {
    QString year;
    qint64 totalTokens;
    double totalCost;
    DateRange range;
    static YearMeta decode(const json::Decoder &decoder);
};
struct UsageMeta {
    QString generatedAt;
    QString version;
    DateRange dateRange;
    static UsageMeta decode(const json::Decoder &decoder);
};
struct UsageSummary {
    qint64 totalTokens;
    double totalCost;
    qint64 totalDays;
    qint64 activeDays;
    double averagePerDay;
    double maxCostInSingleDay;
    QList<QString> clients;
    QList<QString> models;
    static UsageSummary decode(const json::Decoder &decoder);
};
struct UsagePayload {
    using Meta = UsageMeta;
    using Summary = UsageSummary;
    UsageMeta meta;
    UsageSummary summary;
    QList<YearMeta> years;
    QList<Contribution> contributions;
    static std::optional<QString> topVisibleClient(const QList<ContributionClient> &clients,
        const QSet<QString> &hidden, const std::optional<QSet<QString>> &only = std::nullopt);
    TrayTotals trayTotals(const QSet<QString> &hidden, const QString &today,
        const std::optional<QSet<QString>> &only = std::nullopt) const;
    static UsagePayload decode(const json::Decoder &decoder);
};
}
