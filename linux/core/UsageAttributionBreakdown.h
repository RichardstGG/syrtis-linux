#pragma once
#include "UsageAttribution.h"

namespace syrtis::core::UsageAttributionBreakdown {
namespace Copy {
extern const QString title, hint, noUsage, unavailable;
QStringList all();
}
struct Row {
    UsageAttribution::State state;
    qint64 tokens;
    double cost;
    QString id() const;
    bool operator==(const Row &) const = default;
};
QList<Row> rows(const QList<ModelReportEntry> &entries, const QStringList &clientIds,
                const QList<UsageAttribution::Record> &confirmed);
}
