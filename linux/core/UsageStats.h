#pragma once
#include "Graph.h"
#include "Grid.h" // Reuse the ISODay and PerDay port introduced with Grid.

namespace syrtis::core {
struct Streaks {
    qint64 longest = 0;
    qint64 current = 0;
    static Streaks compute(const QHash<QString, PerDay> &perDayMap,
                          const QString &rangeStart, const QString &rangeEnd);
};
struct BestDay { QString date; double cost; };
struct UsageStats {
    qint64 totalTokens = 0;
    double totalCost = 0;
    qint64 activeDays = 0;
    std::optional<BestDay> bestDay;
    double averagePerDay = 0;
    DateRange dateRange;
    QList<PerDay> perDay;
    QHash<QString, PerDay> perDayMap;
    Streaks streaks;
    QStringList presentClients;
    qint64 maxTokens = 0;
    UsageStats(const UsagePayload &payload, const QSet<QString> &selectedClients);
    static QSet<QString> yearsWithVisibleActivity(const QList<Contribution> &contributions,
                                                 const QSet<QString> &hidden);
    static bool hasVisibleActivity(const QList<Contribution> &contributions, const QSet<QString> &hidden);
};
}
