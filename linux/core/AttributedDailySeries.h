#pragma once
#include "Graph.h"
#include "UsageAttribution.h"
#include <QtCore/QMap>

namespace syrtis::core {
namespace AttributedDailySeries {
struct Point {
    QString date;
    UsageAttribution::State state;
    QString model;
    qint64 tokens;
    double cost;
    bool operator==(const Point &other) const;
};
QList<Point> points(const QList<Contribution> &contributions, const QList<UsageAttribution::Record> &confirmed);
}
}
