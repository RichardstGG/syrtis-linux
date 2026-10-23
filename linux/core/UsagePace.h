#pragma once

#include "AgentUsage.h"

namespace syrtis::core {
enum class PaceMode { historical, linear, off };
enum class PaceStage { onTrack, slightlyAhead, ahead, farAhead, slightlyBehind, behind, farBehind };
enum class UsagePaceBasis { linear, historical };

bool isDeficit(PaceStage stage);
std::optional<double> parseRFC3339(const QString &text);

struct UsagePacePresentation {
    std::optional<QString> etaText;
    std::optional<QString> riskText;
};

struct UsagePace {
    PaceStage stage;
    UsagePaceBasis basis;
    double deltaPercent;
    double expectedUsedPercent;
    double actualUsedPercent;
    std::optional<double> etaSeconds;
    bool willLastToReset;

    bool isHistoricalDeficit() const;
    QString label() const;
    std::optional<QString> etaText() const;
    static QString nowText();
    static QString durationText(double seconds);
    // Dates are seconds since 2001-01-01 UTC, matching Foundation.Date.
    static std::optional<QString> spanText(double reset, double now);
    static std::optional<QString> resetText(const QString &resetsAt, double now);
    static std::optional<UsagePace> compute(const UsageWindow &window, double now);
    static std::optional<UsagePace> compute(const UsageWindow &window, PaceMode mode, double now);
    static UsagePacePresentation presentation(const UsageWindow &window, PaceMode mode,
                                              const UsagePace &pace);
};

std::optional<QString> runOutRiskLabel(const UsageWindow &window,
                                     const std::optional<UsagePace> &pace = std::nullopt);
}
