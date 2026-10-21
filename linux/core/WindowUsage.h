#pragma once
#include "TokenBreakdown.h"
#include "UsageAttribution.h"

namespace syrtis::core {
// Per-message quota-window values from WindowUsage.swift.
struct WindowMessage {
    qint64 timestamp = 0;
    QString client, providerId, modelId;
    qint64 input = 0, output = 0, cacheRead = 0, cacheWrite = 0, reasoning = 0;
    double cost = 0;
    bool isTurnStart = false;
    qint64 tokens() const;
    qint64 tokensExCacheRead() const;
    static WindowMessage decode(const json::Decoder &d);
    bool operator==(const WindowMessage &) const = default;
};
struct WindowUsage {
    QList<WindowMessage> messages;
    qint64 undatedCount = 0, processingTimeMs = 0;
    struct SubscriptionTotal {
        QString target;
        qint64 tokens = 0;
        double cost = 0;
        bool operator==(const SubscriptionTotal &other) const;
    };
    struct Totals {
        QList<SubscriptionTotal> assigned;
        SubscriptionTotal excluded{"excluded"}, unassigned{"unassigned"};
    };
    static WindowUsage decode(const json::Decoder &d);
    Totals totals(const QList<UsageAttribution::Record> &confirmed) const;
    QList<qint64> buckets(qint64 from, qint64 bucketMs, qsizetype count) const;
};
}
