#include "QuotaResolver.h"
#include <algorithm>
#include <cmath>

namespace syrtis::core {
namespace {
bool equal(const QString &a, const QString &b)
{
    return a.normalized(QString::NormalizationForm_C) == b.normalized(QString::NormalizationForm_C);
}
std::optional<std::pair<QString, QString>> parse(const QString &raw)
{
    const auto separator = raw.indexOf('|');
    if (separator < 0) return std::nullopt;
    const auto client = raw.left(separator), value = raw.mid(separator + 1);
    if (client.trimmed().isEmpty() || value.trimmed().isEmpty()) return std::nullopt;
    return std::pair{client, value};
}
std::optional<ResolvedQuota> autoCandidate(const AgentUsagePayload &payload, const QSet<QString> &excluding)
{
    std::optional<ResolvedQuota> best;
    for (const auto &agent : payload.agents) {
        if (agent.error || std::any_of(excluding.begin(), excluding.end(),
                [&](const auto &id) { return equal(id, agent.clientId); })) continue;
        for (const auto &window : agent.uniqueCardWindows()) {
            if (std::isfinite(window.remainingPercent) && (!best || window.remainingPercent < best->window.remainingPercent))
                best = ResolvedQuota{agent.clientId, agent.accountKey, window};
        }
    }
    return best;
}
const AgentUsageSnapshot *account(const AgentUsagePayload &payload, const QString &clientId,
                                 const std::optional<QString> &accountKey)
{
    for (const auto &agent : payload.agents)
        if (agent.accountIdentity() == AccountIdentity{clientId, accountKey}) return &agent;
    return nullptr;
}
}

QString QuotaResolver::selection(const QString &clientId, const QString &cardId)
{
    return clientId + '|' + cardId;
}

QString QuotaResolver::canonicalSelection(const std::optional<AgentUsagePayload> &payload,
    const QString &raw, const std::optional<QString> &accountKey)
{
    const auto parsed = parse(raw);
    if (!parsed) return automatic;
    if (!payload) return raw;
    const auto agent = account(*payload, parsed->first, accountKey);
    if (!agent) return raw;
    // Migration compares raw labels before presentation qualifies duplicates.
    const auto windows = agent->rawCardWindows();
    for (const auto &window : windows)
        if (equal(window.cardId, parsed->second)) return selection(agent->clientId, window.cardId);
    const UsageWindow *match = nullptr;
    for (const auto &window : windows) {
        if (!equal(window.label, parsed->second)) continue;
        if (match) return raw;
        match = &window;
    }
    return match ? selection(agent->clientId, match->cardId) : raw;
}

std::optional<ResolvedQuota> QuotaResolver::resolve(const std::optional<AgentUsagePayload> &payload,
    const QString &raw, const std::optional<QString> &accountKey, const QSet<QString> &excluding)
{
    if (!payload) return std::nullopt;
    const auto canonical = canonicalSelection(payload, raw, accountKey);
    if (canonical == automatic) return autoCandidate(*payload, excluding);
    const auto parsed = parse(canonical);
    if (!parsed) return std::nullopt;
    const auto agent = account(*payload, parsed->first, accountKey);
    if (!agent) return std::nullopt;
    for (const auto &window : agent->uniqueCardWindows())
        if (equal(window.cardId, parsed->second)) return ResolvedQuota{agent->clientId, agent->accountKey, window};
    return std::nullopt;
}

bool QuotaResolver::excludedAllCandidates(const std::optional<AgentUsagePayload> &payload,
    const QString &raw, const QSet<QString> &excluding)
{
    return !excluding.isEmpty() && payload && canonicalSelection(payload, raw) == automatic
        && autoCandidate(*payload, {}).has_value() && !autoCandidate(*payload, excluding);
}
}
