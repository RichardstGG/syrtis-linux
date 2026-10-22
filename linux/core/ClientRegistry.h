#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <optional>

namespace syrtis::core {
struct ClientStyle {
    QString id;
    QString displayName;
    QString color;
};

// Snapshot supplied by the settings owner. Core never reads or writes storage.
struct ClientPreferences {
    std::optional<QString> order;
    QString hidden;
    QString limitsHidden;
    std::optional<QString> legacyOrder;
};

namespace ClientRegistry {
inline const QString tabOrderKey = QStringLiteral("tokenbar.tabs.order");
inline const QString tabHiddenKey = QStringLiteral("tokenbar.tabs.hidden");
inline const QString limitsHiddenKey = QStringLiteral("tokenbar.limits.hidden");
extern const QHash<QString, QString> subscriptionLabelAliases;
QStringList allIds();
ClientStyle style(const QString &id);
QString shortName(const QString &id);
QString canonicalClient(const QString &id);
QString quotaOwner(const QString &id);
QString clientIdForSubscriptionLabel(const QString &label);
QSet<QString> parseIdSet(const QString &raw);
QStringList parseIdList(const QString &raw);
QSet<QString> hiddenClients(const ClientPreferences &preferences = {});
QSet<QString> hiddenLimitsClients(const ClientPreferences &preferences = {});
QSet<QString> quotaExcludedClients(const QSet<QString> &tabHidden, const QSet<QString> &limitsHidden);
QSet<QString> quotaExcludedClients(const ClientPreferences &preferences = {});
QStringList tabSlice(const QString &id);
QStringList tabClients(const QStringList &present, const QStringList &quotaIds);
QStringList quotaClients(const QStringList &present, const QStringList &quotaIds,
                         const QSet<QString> &tabHidden, const QString &orderRaw);
QString tabLabel(const QString &id);
QString tabDisplayName(const QString &id);
QSet<QString> withGroupMembers(const QSet<QString> &ids);
QStringList knownLimitsClients(const QStringList &present, const QStringList &quotaIds,
                              const QSet<QString> &placeholders);
QStringList orderedClients(const QStringList &ids, const QString &orderRaw);
QStringList orderedClients(const QStringList &ids, const ClientPreferences &preferences = {});
QSet<QString> hiddenTabClients(const QSet<QString> &raw);
QSet<QString> hiddenTabClients(const ClientPreferences &preferences = {});
QStringList displayClients(const QStringList &present, const QString &hiddenRaw, const QString &orderRaw);
QStringList displayClients(const QStringList &present, const ClientPreferences &preferences = {});
QStringList reorder(const QStringList &list, const QString &from, const QString &to);
QStringList mergeReorder(const QStringList &full, const QStringList &visible,
                        const QString &from, const QString &to);
void migrateLegacyOrderKey(ClientPreferences &preferences);
}
}
