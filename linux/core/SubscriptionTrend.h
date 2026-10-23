#pragma once
#include "AttributedDailySeries.h"

namespace syrtis::core {
struct SubscriptionTrend {
    struct Bucket {
        qint64 tokens = 0;
        double cost = 0;
        bool operator==(const Bucket &) const = default;
    };
    struct Day {
        QString date;
        QMap<QString, Bucket> byTarget;
        qint64 totalTokens = 0;
        double totalCost = 0;
        bool isEmpty() const { return totalTokens == 0 && totalCost == 0; }
        bool operator==(const Day &other) const;
    };
    QList<Day> days;
    QStringList targets, targetsByTokens;
    double peakCost = 0;
    qint64 peakTokens = 0;
    QStringList targetsFor(bool byTokens) const { return byTokens ? targetsByTokens : targets; }
    static const SubscriptionTrend empty;
    bool operator==(const SubscriptionTrend &other) const;
};
namespace SubscriptionTrendFold {
inline const QString unassignedTarget = "__unassigned";
SubscriptionTrend build(const QList<AttributedDailySeries::Point> &points, const QString &today, qsizetype days);
std::optional<QStringList> calendarRange(const QString &endingAt, qsizetype count);
}
}
