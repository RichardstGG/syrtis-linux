#include "QuotaSelectionPolicy.h"
#include <cmath>

namespace Syrtis {
QString QuotaSelectionPolicy::effectiveSelection(const std::optional<AgentUsagePayload> &payload,
    const QString &persistedSelection, const QSet<QString> &, const std::optional<QString> &accountKey)
{
    return QuotaResolver::canonicalSelection(payload, persistedSelection, accountKey);
}

std::optional<QString> QuotaSelectionPolicy::migrationToPersist(
    const std::optional<AgentUsagePayload> &payload, const QString &persistedSelection,
    const std::optional<QString> &accountKey)
{
    const auto canonical = effectiveSelection(payload, persistedSelection, {}, accountKey);
    return canonical != QuotaResolver::automatic && canonical != persistedSelection
        ? std::optional<QString>(canonical) : std::nullopt;
}

std::optional<ResolvedQuota> QuotaSelectionPolicy::resolve(const std::optional<AgentUsagePayload> &payload,
    const QString &persistedSelection, const QSet<QString> &excluding,
    const std::optional<QString> &accountKey)
{
    return QuotaResolver::resolve(payload,
        effectiveSelection(payload, persistedSelection, excluding, accountKey), accountKey, excluding);
}

std::optional<double> QuotaSelectionPolicy::resolveRemainingPercent(
    const std::optional<AgentUsagePayload> &payload, const QString &persistedSelection,
    const QSet<QString> &excluding, std::optional<double> cachedRemaining,
    const std::optional<QString> &accountKey)
{
    if (!payload) return cachedRemaining;
    const auto reading = resolve(payload, persistedSelection, excluding, accountKey);
    return reading && std::isfinite(reading->window.remainingPercent)
        ? std::optional<double>(reading->window.remainingPercent) : std::nullopt;
}

std::optional<QDateTime> QuotaSelectionPolicy::resolvedAt(const AgentUsagePayload &payload,
    const QString &persistedSelection, const QSet<QString> &excluding,
    const std::optional<QString> &accountKey)
{
    const auto reading = resolve(payload, persistedSelection, excluding, accountKey);
    if (!reading) return std::nullopt;
    for (const auto &agent : payload.agents) {
        if (agent.accountIdentity() != AccountIdentity{reading->clientId, reading->accountKey}) continue;
        const auto date = QDateTime::fromString(agent.updatedAt, Qt::ISODateWithMs);
        return date.isValid() ? std::optional<QDateTime>(date) : std::nullopt;
    }
    return std::nullopt;
}
} // namespace Syrtis
