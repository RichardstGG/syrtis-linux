#pragma once
#include "AccountIdentity.h"
#include "QuotaCycle.h"
#include <QtCore/QList>

namespace syrtis::core {
struct QuotaWindowSummary {
    QString clientId;
    std::optional<QString> accountKey;
    QString cardId, windowLabel;
    QList<double> recent, recentPeaks;
    double peakPercent;
    bool neverExhausted;
    qsizetype cycleCount;
    QString id() const { return AccountIdentity{clientId, accountKey}.windowKey(cardId); }
    bool operator==(const QuotaWindowSummary &) const = default;
};
struct QuotaHeatmapWindow {
    QString clientId, cardId, windowLabel;
    double total;
    std::optional<QString> accountKey;
    QString id() const { return AccountIdentity{clientId, accountKey}.windowKey(cardId); }
    bool operator==(const QuotaHeatmapWindow &) const = default;
};
struct QuotaOverviewWindow {
    QString clientId;
    std::optional<QString> accountKey;
    QString cardId, label;
    QList<QuotaCycle> cycles;
};
struct QuotaOverviewFold {
    static constexpr double exhaustedPercent = 99;
    static constexpr qsizetype stripLength = 16;
    static QList<QuotaWindowSummary> summaries(const QList<QuotaOverviewWindow> &windows);
};
}
