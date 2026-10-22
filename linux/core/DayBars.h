#pragma once
#include "Graph.h"
#include "ModelColors.h"

namespace syrtis::core {
enum class StackBy { model, agent };
enum class ChartMetric { tokens, cost };
struct DaySegment {
    QString key;
    QString label;
    QString color;
    qint64 tokens = 0;
    double cost = 0;
};
struct DayBar {
    QString date;
    QList<DaySegment> segments;
    qint64 totalTokens() const;
    double totalCost() const;
    bool isEmpty() const { return segments.isEmpty(); }
};
namespace DayBars {
inline constexpr int window = 30;
QList<DayBar> build(const UsagePayload &payload, const QStringList &clientIds, StackBy stackBy,
                   const ModelColorMap &colors, const QString &rangeEnd, const QString &endFallback);
DayBar dayBar(const Contribution &contribution, const QSet<QString> &allowed, StackBy stackBy,
              const ModelColorMap &colors);
QList<DaySegment> legend(const QList<DayBar> &bars, ChartMetric metric);
}
}
