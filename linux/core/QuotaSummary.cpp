#include "QuotaSummary.h"
#include <algorithm>
#include <cmath>

namespace Syrtis {
std::optional<QuotaSummary> QuotaSummaryFold::build(const std::optional<AgentUsagePayload> &payload,
    const QSet<QString> &excluding, PaceMode paceMode, double now)
{
    const auto tightest = QuotaResolver::resolve(payload, QuotaResolver::automatic, std::nullopt, excluding);
    if (!tightest) return std::nullopt;
    QuotaSummary result{tightest->clientId, tightest->accountKey, tightest->window.label,
        tightest->window.remainingPercent, tightest->window.resetsAt, 0, 0, std::nullopt, 0};
    const AccountIdentity tightestIdentity{tightest->clientId, tightest->accountKey};
    for (const auto &agent : payload->agents) {
        if (agent.error || std::any_of(excluding.begin(), excluding.end(), [&](const auto &id) {
                return id.normalized(QString::NormalizationForm_C) == agent.clientId.normalized(QString::NormalizationForm_C);
            })) continue;
        for (const auto &window : agent.uniqueCardWindows()) {
            if (!std::isfinite(window.remainingPercent)) continue;
            const auto pace = UsagePace::compute(window, paceMode, now);
            if (pace) ++result.paceCheckedWindows;
            if (pace && isDeficit(pace->stage) && pace->deltaPercent > (result.burning ? result.burning->aheadPercent : 0)) {
                const auto shown = UsagePace::presentation(window, paceMode, *pace);
                result.burning = BurnWarning{agent.clientId, agent.accountKey, window.label,
                    pace->deltaPercent, shown.etaText, shown.riskText};
            }
            if (agent.accountIdentity() == tightestIdentity
                && window.cardId.normalized(QString::NormalizationForm_C) == tightest->window.cardId.normalized(QString::NormalizationForm_C)) continue;
            ++result.otherWindows;
            if (window.remainingPercent >= comfortablePercent) ++result.othersComfortable;
        }
    }
    return result;
}
}
