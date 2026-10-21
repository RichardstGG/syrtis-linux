#pragma once

#include "JsonDecoder.h"
#include <QtCore/QString>

#include <optional>

namespace Syrtis {
// UI-free AgentUsage.swift values; no FFI, provider calls or aggregation.
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
    static HistoricalPace decode(const syrtis::core::json::Decoder &decoder);
};

struct PaceStatus {
    UsagePaceState state = UsagePaceState::legacyMissing;
    std::optional<QString> windowKey;
    std::optional<qint64> durationSeconds;
    std::optional<UsagePaceDurationSource> durationSource;
    qint64 completeCycles = 0;
    std::optional<UsagePaceUnavailableReason> reason;
    std::optional<QString> historyKey() const;
    static PaceStatus decode(const syrtis::core::json::Decoder &decoder);
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

    UsageWindow replacingPace(const UsageWindow &other) const;
    static UsageWindow decode(const syrtis::core::json::Decoder &decoder);
    // Compatibility entry point used by UsagePace selectors.
    static std::optional<UsageWindow> decode(const QJsonValue &value);
};

struct AgentIdentity {
    std::optional<QString> email, plan;
    static AgentIdentity decode(const syrtis::core::json::Decoder &decoder);
};
struct CreditsSnapshot {
    std::optional<double> remaining;
    bool unlimited = false;
    static CreditsSnapshot decode(const syrtis::core::json::Decoder &decoder);
};
struct AgentUsageTransportDiagnostic {
    std::optional<QString> category;
    std::optional<qint64> status, osCode;
    static AgentUsageTransportDiagnostic decode(const syrtis::core::json::Decoder &decoder);
};
struct SetupInstructions {
    enum class Kind { claudeSetupToken, providerMessage, none };
    Kind kind = Kind::none;
    QString message;
};
struct AgentUsageSnapshot {
    QString clientId;
    std::optional<QString> accountKey;
    QString source, updatedAt;
    std::optional<AgentIdentity> identity;
    QList<UsageWindow> windows;
    std::optional<CreditsSnapshot> credits;
    std::optional<QString> error;
    std::optional<AgentUsageTransportDiagnostic> transportDiagnostic;
    std::optional<QString> agyLoginMarker, boundAccountKey;
    // Display-only rewrites never decode this field from the wire.
    std::optional<QString> historyAccountKey;

    std::optional<QString> historyReadAccountKey() const;
    bool isSetupPlaceholder() const;
    std::optional<QString> setupBadgeKey() const;
    SetupInstructions setupInstructions() const;
    QList<UsageWindow> rawCardWindows() const;
    QList<UsageWindow> uniqueCardWindows() const;
    // Dates use Foundation's seconds since 2001-01-01, as in UsagePace.
    static QList<UsageWindow> qualifyingRepeatedLabels(const QList<UsageWindow> &windows, double now);
    AgentUsageSnapshot replacingIdentity(const std::optional<AgentIdentity> &identity) const;
    AgentUsageSnapshot adoptingHistory(const AgentUsageSnapshot &captured) const;
    static AgentUsageSnapshot decode(const syrtis::core::json::Decoder &decoder);
};
struct AgentUsagePayload {
    QString generatedAt;
    std::optional<quint64> publicationGeneration;
    QList<AgentUsageSnapshot> agents;
    std::optional<QStringList> opencodeSubscriptions;
    QStringList configuredClientIds() const;
    AgentUsagePayload replacingAgents(const QList<AgentUsageSnapshot> &agents) const;
    static AgentUsagePayload decode(const syrtis::core::json::Decoder &decoder);
};
struct QuotaProviderIds {
    QStringList ids;
    static QuotaProviderIds decode(const syrtis::core::json::Decoder &decoder);
};
}
