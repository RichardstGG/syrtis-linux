#pragma once
#include <QtCore/QString>
#include <QtCore/QHashFunctions>
#include <functional>
#include <optional>

namespace syrtis::core {
struct AccountIdentity {
    QString clientId;
    std::optional<QString> accountKey;
    bool isPrimary() const { return !accountKey; }
    QString windowKey(const QString &cardId) const;
    std::optional<QString> accountLabel() const;
    std::optional<QString> accountTooltip() const;
    bool hasLocalUsage() const { return !accountKey || clientId == "claude"; }
    static inline const QString antigravityClientId = QStringLiteral("antigravity");
    // Installed by the settings owner at startup, as in Swift.
    static std::function<std::optional<QString>(const QString &)> antigravityLabel;
    bool operator==(const AccountIdentity &other) const;
};
size_t qHash(const AccountIdentity &identity, size_t seed = 0);
}
