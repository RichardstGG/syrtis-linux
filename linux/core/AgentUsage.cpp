#include "AgentUsage.h"
#include "Localization.h"
#include "UsagePace.h"

#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QSet>

namespace Syrtis {
namespace {
using syrtis::core::json::Decoder;
using syrtis::core::DecodingError;
// Swift String/Set equality uses canonical equivalence; retain the original
// spelling for display and use NFC only for identity/group membership.
QString key(const QString &value) { return value.normalized(QString::NormalizationForm_C); }
void require(bool condition, const Decoder &d)
{
    if (!condition) d.fail("dataCorrupted");
}

template<typename T> T enumeration(const Decoder &d, std::initializer_list<std::pair<const char *, T>> values)
{
    const auto raw = d.decode<QString>();
    for (const auto &[name, value] : values)
        if (raw == QLatin1String(name)) return value;
    d.fail("dataCorrupted");
}

Decoder child(const Decoder &d, const QString &key)
{
    d.object();
    if (!d.value.toObject().contains(key)) throw DecodingError("keyNotFound", d.path, key);
    return {d.value.toObject()[key], d.path + QStringList{key}};
}

template<class T> std::optional<T> forgivingField(const Decoder &d, const QString &key)
{
    try { return d.field<T>(key); }
    catch (const DecodingError &) { return std::nullopt; }
}

bool validV3(const PaceStatus &status, const std::optional<qint64> &minutes,
             const std::optional<qint64> &duration, const std::optional<HistoricalPace> &history)
{
    if (duration ? minutes != *duration / 60 : minutes.has_value()) return false;
    switch (status.state) {
    case UsagePaceState::available: return duration && *duration > 0 && history;
    case UsagePaceState::learningHistory: return duration && *duration > 0 && !history;
    case UsagePaceState::learningDuration: return !duration && !history;
    case UsagePaceState::unavailable: return !history;
    case UsagePaceState::legacyMissing: return false;
    }
    return false;
}
}

HistoricalPace HistoricalPace::decode(const Decoder &d)
{
    d.object();
    HistoricalPace result{d.field<double>("expectedUsedPercent"),
        d.field<std::optional<double>>("etaSeconds"), d.field<bool>("willLastToReset"),
        d.field<std::optional<double>>("runOutProbability")};
    require(std::isfinite(result.expectedUsedPercent) && result.expectedUsedPercent >= 0 && result.expectedUsedPercent <= 100, d);
    require(!result.etaSeconds || (std::isfinite(*result.etaSeconds) && *result.etaSeconds >= 0), d);
    require(!result.runOutProbability || (std::isfinite(*result.runOutProbability) && *result.runOutProbability >= 0 && *result.runOutProbability <= 1), d);
    require(!result.etaSeconds == result.willLastToReset, d);
    return result;
}

PaceStatus PaceStatus::decode(const Decoder &d)
{
    d.object();
    PaceStatus result;
    using State = UsagePaceState;
    result.state = enumeration<State>(child(d, "state"), {
        {"learningDuration", State::learningDuration}, {"learningHistory", State::learningHistory},
        {"available", State::available}, {"unavailable", State::unavailable}});
    result.windowKey = d.field<std::optional<QString>>("windowKey");
    result.durationSeconds = d.field<std::optional<qint64>>("durationSeconds");
    if (d.field<std::optional<QString>>("durationSource")) {
        using Source = UsagePaceDurationSource;
        result.durationSource = enumeration<Source>(child(d, "durationSource"), {
            {"provider", Source::provider}, {"contract", Source::contract}, {"observed", Source::observed}});
    }
    result.completeCycles = d.field<qint64>("completeCycles");
    if (d.field<std::optional<QString>>("reason")) {
        using Reason = UsagePaceUnavailableReason;
        result.reason = enumeration<Reason>(child(d, "reason"), {
            {"windowIdentity", Reason::windowIdentity}, {"missingReset", Reason::missingReset},
            {"invalidEvidence", Reason::invalidEvidence}, {"accountScope", Reason::accountScope},
            {"storeCapacity", Reason::storeCapacity}, {"history", Reason::history},
            {"nonRecurring", Reason::nonRecurring}});
    }
    require(result.completeCycles >= 0, d);
    const bool identityUnavailable = result.state == State::unavailable
        && result.reason == UsagePaceUnavailableReason::windowIdentity;
    require(!result.windowKey == identityUnavailable, d);
    require(!result.windowKey || !result.windowKey->trimmed().isEmpty(), d);
    if (result.durationSeconds) {
        require(*result.durationSeconds > 0 && *result.durationSeconds <= 400 * 86400, d);
        require(result.durationSource.has_value(), d);
    } else if (result.durationSource) {
        require(result.state == State::learningDuration
                && result.durationSource == UsagePaceDurationSource::observed, d);
    }
    switch (result.state) {
    case State::learningDuration: require(!result.durationSeconds && !result.reason, d); break;
    case State::learningHistory:
    case State::available: require(result.durationSeconds && result.durationSource && !result.reason, d); break;
    case State::unavailable:
        require(result.reason.has_value(), d);
        require(result.durationSeconds || !result.durationSource, d);
        break;
    case State::legacyMissing: require(false, d);
    }
    return result;
}

std::optional<QString> PaceStatus::historyKey() const
{
    return state == UsagePaceState::unavailable && reason == UsagePaceUnavailableReason::accountScope
        ? std::nullopt : windowKey;
}

UsageWindow UsageWindow::decode(const Decoder &d)
{
    d.object();
    const auto object = d.value.toObject();
    UsageWindow result;
    result.label = d.field<QString>("label");
    result.usedPercent = d.field<double>("usedPercent");
    result.remainingPercent = d.field<double>("remainingPercent");
    result.resetsAt = d.field<std::optional<QString>>("resetsAt");
    result.resetText = d.field<std::optional<QString>>("resetText");
    result.windowMinutes = d.field<std::optional<qint64>>("windowMinutes");
    result.historicalPace = d.field<std::optional<HistoricalPace>>("historicalPace");
    result.modelScope = d.field<std::optional<QString>>("modelScope");
    require(std::isfinite(result.usedPercent) && std::isfinite(result.remainingPercent)
            && result.usedPercent >= 0 && result.usedPercent <= 100
            && result.remainingPercent >= 0 && result.remainingPercent <= 100, d);
    require(std::abs(result.usedPercent + result.remainingPercent - 100) < 0.000001, d);
    const bool v3 = object.contains("paceStatus");
    if (v3 || object.contains("cardId")) {
        result.cardId = d.field<QString>("cardId");
        require(!result.cardId.trimmed().isEmpty(), d);
    }
    if (!v3) return result;
    result.paceStatus = d.field<PaceStatus>("paceStatus");
    // Top-level durationSeconds is deliberately not a Swift CodingKey.
    result.durationSeconds = result.paceStatus.durationSeconds;
    require(validV3(result.paceStatus, result.windowMinutes, result.durationSeconds, result.historicalPace), d);
    return result;
}

std::optional<UsageWindow> UsageWindow::decode(const QJsonValue &value)
{
    try { return decode(Decoder{value, {}}); }
    catch (const DecodingError &) { return std::nullopt; }
}

UsageWindow UsageWindow::replacingPace(const UsageWindow &other) const
{
    if ((durationSeconds && durationSeconds != other.durationSeconds)
        || other.durationSeconds != other.paceStatus.durationSeconds
        || cardId.trimmed().isEmpty()
        || !validV3(other.paceStatus, other.windowMinutes, other.durationSeconds, other.historicalPace)) return *this;
    auto copy = *this;
    copy.paceStatus = other.paceStatus;
    copy.historicalPace = other.historicalPace;
    copy.durationSeconds = other.durationSeconds;
    copy.windowMinutes = other.windowMinutes;
    return copy;
}

AgentIdentity AgentIdentity::decode(const Decoder &d)
{
    d.object();
    return {d.field<std::optional<QString>>("email"), d.field<std::optional<QString>>("plan")};
}
CreditsSnapshot CreditsSnapshot::decode(const Decoder &d)
{
    d.object();
    return {d.field<std::optional<double>>("remaining"), d.field<bool>("unlimited")};
}
AgentUsageTransportDiagnostic AgentUsageTransportDiagnostic::decode(const Decoder &d)
{
    d.object();
    return {forgivingField<QString>(d, "category"), forgivingField<qint64>(d, "status"),
            forgivingField<qint64>(d, "osCode")};
}
AgentUsageSnapshot AgentUsageSnapshot::decode(const Decoder &d)
{
    d.object();
    AgentUsageSnapshot result;
    result.clientId = d.field<QString>("clientId");
    result.accountKey = d.field<std::optional<QString>>("accountKey");
    result.source = d.field<QString>("source");
    result.updatedAt = d.field<QString>("updatedAt");
    result.identity = d.field<std::optional<AgentIdentity>>("identity");
    result.windows = d.field<QList<UsageWindow>>("windows");
    result.credits = d.field<std::optional<CreditsSnapshot>>("credits");
    result.error = d.field<std::optional<QString>>("error");
    result.transportDiagnostic = forgivingField<AgentUsageTransportDiagnostic>(d, "transportDiagnostic");
    result.agyLoginMarker = d.field<std::optional<QString>>("agyLoginMarker");
    result.boundAccountKey = d.field<std::optional<QString>>("boundAccountKey");
    return result;
}
std::optional<QString> AgentUsageSnapshot::historyReadAccountKey() const
{
    return historyAccountKey ? historyAccountKey : accountKey;
}
std::optional<QString> AgentUsageSnapshot::setupBadgeKey() const
{
    if (source == "unconfigured") return "Set up";
    if (source == "keychain-consent" || source == "keychain-denied") return "Allow";
    return std::nullopt;
}
bool AgentUsageSnapshot::isSetupPlaceholder() const { return setupBadgeKey().has_value(); }
SetupInstructions AgentUsageSnapshot::setupInstructions() const
{
    if (source != "unconfigured") return {};
    if (clientId == "claude") return {SetupInstructions::Kind::claudeSetupToken, {}};
    if (!error || error->isEmpty()) return {};
    return {SetupInstructions::Kind::providerMessage, *error};
}
QList<UsageWindow> AgentUsageSnapshot::rawCardWindows() const
{
    QSet<QString> seen;
    QList<UsageWindow> result;
    for (const auto &window : windows) {
        if (seen.contains(key(window.cardId))) continue;
        seen.insert(key(window.cardId));
        result.append(window);
    }
    return result;
}
QList<UsageWindow> AgentUsageSnapshot::uniqueCardWindows() const
{
    return qualifyingRepeatedLabels(rawCardWindows(), QDateTime::currentMSecsSinceEpoch() / 1000.0 - 978307200.0);
}
QList<UsageWindow> AgentUsageSnapshot::qualifyingRepeatedLabels(const QList<UsageWindow> &windows, double now)
{
    QHash<QString, qsizetype> counts;
    for (const auto &window : windows) ++counts[key(window.label)];
    QSet<QString> untouched;
    for (const auto &window : windows)
        if (counts[key(window.label)] == 1) untouched.insert(key(window.label));
    const auto compose = [](const QString &label, const QString &qualifier) {
        return localized("%@ · %@", {localized(label), qualifier});
    };
    const auto tier = [&](auto candidate) {
        QHash<QString, QStringList> byLabel;
        for (const auto &window : windows) {
            if (counts[key(window.label)] <= 1) continue;
            const auto value = candidate(window);
            if (!value) { byLabel[key(window.label)] = {}; continue; }
            if (byLabel.contains(key(window.label)) && byLabel[key(window.label)].isEmpty()) continue;
            byLabel[key(window.label)].append(*value);
        }
        for (auto it = byLabel.begin(); it != byLabel.end();) {
            const auto &values = it.value();
            bool valid = values.size() == counts[it.key()]
                && [&] {
                    QSet<QString> distinct;
                    for (const auto &value : values) distinct.insert(key(value));
                    return distinct.size() == values.size();
                }();
            for (const auto &value : values) valid = valid && !untouched.contains(key(compose(it.key(), value)));
            if (!valid) it = byLabel.erase(it); else ++it;
        }
        return byLabel;
    };
    const auto byLength = tier([](const UsageWindow &w) -> std::optional<QString> {
        if (!w.durationSeconds || *w.durationSeconds <= 0) return std::nullopt;
        if (*w.durationSeconds == 18000) return localized("Session");
        if (*w.durationSeconds == 604800) return localized("Weekly");
        return UsagePace::durationText(double(*w.durationSeconds));
    });
    const auto byReset = tier([now](const UsageWindow &w) -> std::optional<QString> {
        const auto reset = w.resetsAt ? parseRFC3339(*w.resetsAt) : std::nullopt;
        return reset ? UsagePace::spanText(*reset, now) : std::nullopt;
    });
    QHash<QString, qsizetype> taken;
    auto used = untouched;
    auto result = windows;
    for (auto &window : result) {
        if (counts[key(window.label)] <= 1) continue;
        const auto index = taken[key(window.label)]++;
        QString name;
        if (byLength.contains(key(window.label))) name = compose(window.label, byLength[key(window.label)][index]);
        else if (byReset.contains(key(window.label))) name = compose(window.label, byReset[key(window.label)][index]);
        if (name.isNull() || used.contains(key(name))) {
            auto ordinal = index + 1;
            do { name = compose(window.label, QString::number(ordinal++)); } while (used.contains(key(name)));
        }
        used.insert(key(name));
        window.label = name;
    }
    return result;
}
AgentUsageSnapshot AgentUsageSnapshot::replacingIdentity(const std::optional<AgentIdentity> &value) const
{
    auto copy = *this;
    copy.identity = value;
    return copy;
}
AgentUsageSnapshot AgentUsageSnapshot::adoptingHistory(const AgentUsageSnapshot &captured) const
{
    if (captured.error || !captured.accountKey || captured.windows.isEmpty()) return *this;
    auto copy = *this;
    const auto theirs = captured.rawCardWindows();
    for (auto &window : copy.windows) {
        for (const auto &other : theirs) {
            if (key(window.cardId) != key(other.cardId)) continue;
            window = window.replacingPace(other);
            break;
        }
    }
    copy.historyAccountKey = captured.accountKey;
    return copy;
}
AgentUsagePayload AgentUsagePayload::decode(const Decoder &d)
{
    d.object();
    return {d.field<QString>("generatedAt"), d.field<std::optional<quint64>>("publicationGeneration"),
            d.field<QList<AgentUsageSnapshot>>("agents"), d.field<std::optional<QStringList>>("opencodeSubscriptions")};
}
QStringList AgentUsagePayload::configuredClientIds() const
{
    QSet<QString> seen;
    QStringList result;
    for (const auto &agent : agents) {
        if (agent.isSetupPlaceholder() || seen.contains(key(agent.clientId))) continue;
        seen.insert(key(agent.clientId));
        result.append(agent.clientId);
    }
    return result;
}
AgentUsagePayload AgentUsagePayload::replacingAgents(const QList<AgentUsageSnapshot> &value) const
{
    auto copy = *this;
    copy.agents = value;
    return copy;
}
QuotaProviderIds QuotaProviderIds::decode(const Decoder &d)
{
    return {d.field<QStringList>("ids")};
}
}
