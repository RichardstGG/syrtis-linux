#pragma once
#include "QuotaResolver.h"
#include "UsagePace.h"
#include <QtCore/QDateTime>

namespace syrtis::core {
struct BurnWarning {
    QString clientId;
    std::optional<QString> accountKey;
    QString label;
    double aheadPercent;
    std::optional<QString> etaText, riskText;
    bool operator==(const BurnWarning &) const = default;
};
struct QuotaSummary {
    QString tightestClient;
    std::optional<QString> tightestAccountKey;
    QString tightestLabel;
    double remainingPercent;
    std::optional<QString> resetsAt;
    qsizetype otherWindows, othersComfortable;
    std::optional<BurnWarning> burning;
    qsizetype paceCheckedWindows;
    bool allOthersComfortable() const { return otherWindows > 0 && othersComfortable == otherWindows; }
    bool operator==(const QuotaSummary &) const = default;
};
struct QuotaSummaryFold {
    static constexpr double comfortablePercent = 60;
    static std::optional<QuotaSummary> build(const std::optional<AgentUsagePayload> &payload,
        const QSet<QString> &excluding = {}, PaceMode paceMode = PaceMode::historical,
        double now = QDateTime::currentMSecsSinceEpoch() / 1000.0 - 978307200.0);
};
}
