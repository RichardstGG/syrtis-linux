#include "AccountIdentity.h"
#include "Localization.h"

namespace Syrtis {
std::function<std::optional<QString>(const QString &)> AccountIdentity::antigravityLabel =
    [](const QString &) -> std::optional<QString> { return std::nullopt; };

QString AccountIdentity::windowKey(const QString &cardId) const
{
    return clientId + '|' + (accountKey ? *accountKey + '|' : QString{}) + cardId;
}

std::optional<QString> AccountIdentity::accountLabel() const
{
    if (!accountKey) return std::nullopt;
    if (clientId == antigravityClientId)
        return antigravityLabel(*accountKey).value_or(localized("Antigravity account"));
    auto path = *accountKey;
    while (path.size() > 1 && path.endsWith('/')) path.chop(1);
    const auto name = path == "/" ? path : path.section('/', -1);
    return name.isEmpty() ? *accountKey : name;
}

std::optional<QString> AccountIdentity::accountTooltip() const
{
    return clientId == antigravityClientId ? accountLabel() : accountKey;
}

bool AccountIdentity::operator==(const AccountIdentity &other) const
{
    return clientId.normalized(QString::NormalizationForm_C) == other.clientId.normalized(QString::NormalizationForm_C)
        && accountKey.has_value() == other.accountKey.has_value()
        && (!accountKey || accountKey->normalized(QString::NormalizationForm_C) == other.accountKey->normalized(QString::NormalizationForm_C));
}

size_t qHash(const AccountIdentity &identity, size_t seed)
{
    return qHashMulti(seed, identity.clientId.normalized(QString::NormalizationForm_C),
        identity.accountKey.has_value(), identity.accountKey.value_or(QString{}).normalized(QString::NormalizationForm_C));
}
}
