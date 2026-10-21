#pragma once
#include "TokenBreakdown.h"
#include "UsageAttribution.h"
#include <QtCore/QMap>

namespace syrtis::core {
// Graph.swift value dependencies; Rust remains the owner of these aggregates.
struct ContributionClient {
    QString client, modelId, providerId;
    TokenBreakdown tokens;
    double cost;
    qint64 messages;
    static ContributionClient decode(const json::Decoder &d);
};
struct Contribution {
    struct Totals {
        qint64 tokens;
        double cost;
        qint64 messages;
        static Totals decode(const json::Decoder &d);
    };
    QString date;
    Totals totals;
    qint64 intensity;
    TokenBreakdown tokenBreakdown;
    QList<ContributionClient> clients;
    std::optional<QMap<QString, qint64>> turnsByClient;
    static Contribution decode(const json::Decoder &d);
};
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
