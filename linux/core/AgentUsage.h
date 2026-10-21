#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <optional>

namespace Syrtis {
// Pace inputs from AgentUsage.swift; no provider selection or aggregation.
enum class UsagePaceState { learningDuration, learningHistory, available, unavailable, legacyMissing };
enum class UsagePaceDurationSource { provider, contract, observed };
enum class UsagePaceUnavailableReason {
    windowIdentity, missingReset, invalidEvidence, accountScope, storeCapacity, history, nonRecurring
};

struct HistoricalPace {
    double expectedUsedPercent = 0;
    std::optional<double> etaSeconds;
    bool willLastToReset = false;
    std::optional<double> runOutProbability;
};

struct PaceStatus {
    UsagePaceState state = UsagePaceState::legacyMissing;
    std::optional<QString> windowKey;
    std::optional<qint64> durationSeconds;
    std::optional<UsagePaceDurationSource> durationSource;
    qint64 completeCycles = 0;
    std::optional<UsagePaceUnavailableReason> reason;
};

struct UsageWindow {
    QString cardId = QStringLiteral("legacy.missing.v1");
    QString label;
    double usedPercent = 0;
    double remainingPercent = 100;
    std::optional<QString> resetsAt;
    std::optional<QString> resetText;
    std::optional<qint64> windowMinutes;
    std::optional<qint64> durationSeconds;
    PaceStatus paceStatus;
    std::optional<HistoricalPace> historicalPace;
    std::optional<QString> modelScope;

    // Same required fields, null handling and v3 invariants as Swift's decoder.
    static std::optional<UsageWindow> decode(const QJsonValue &value);
};
}
