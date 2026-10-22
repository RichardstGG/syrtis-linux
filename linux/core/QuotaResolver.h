#pragma once
#include "AgentUsage.h"
#include <QtCore/QSet>

namespace Syrtis {
struct ResolvedQuota {
    QString clientId;
    std::optional<QString> accountKey;
    UsageWindow window;
};
struct QuotaResolver {
    static inline const QString automatic = QStringLiteral("auto"); // `auto` is a C++ keyword.
    static QString selection(const QString &clientId, const QString &cardId);
    static QString canonicalSelection(const std::optional<AgentUsagePayload> &payload,
        const QString &selection, const std::optional<QString> &accountKey = std::nullopt);
    static std::optional<ResolvedQuota> resolve(const std::optional<AgentUsagePayload> &payload,
        const QString &selection, const std::optional<QString> &accountKey = std::nullopt,
        const QSet<QString> &excluding = {});
    static bool excludedAllCandidates(const std::optional<AgentUsagePayload> &payload,
        const QString &selection, const QSet<QString> &excluding);
};
}
