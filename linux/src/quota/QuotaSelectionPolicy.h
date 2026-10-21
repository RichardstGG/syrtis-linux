#pragma once

#include "QuotaResolver.h"
#include <QDateTime>

namespace Syrtis {
// Port of Sources/Syrtis/QuotaSelectionPolicy.swift. Selection and migration
// stay in syrtis-core; accountKey extends the primary-account Swift caller.
struct QuotaSelectionPolicy {
    static QString effectiveSelection(const std::optional<AgentUsagePayload> &payload,
        const QString &persistedSelection, const QSet<QString> &excluding = {},
        const std::optional<QString> &accountKey = std::nullopt);
    static std::optional<QString> migrationToPersist(const std::optional<AgentUsagePayload> &payload,
        const QString &persistedSelection, const std::optional<QString> &accountKey = std::nullopt);
    static std::optional<ResolvedQuota> resolve(const std::optional<AgentUsagePayload> &payload,
        const QString &persistedSelection, const QSet<QString> &excluding = {},
        const std::optional<QString> &accountKey = std::nullopt);
    static std::optional<double> resolveRemainingPercent(const std::optional<AgentUsagePayload> &payload,
        const QString &persistedSelection, const QSet<QString> &excluding,
        std::optional<double> cachedRemaining, const std::optional<QString> &accountKey = std::nullopt);
    static std::optional<QDateTime> resolvedAt(const AgentUsagePayload &payload,
        const QString &persistedSelection, const QSet<QString> &excluding = {},
        const std::optional<QString> &accountKey = std::nullopt);
};
} // namespace Syrtis
