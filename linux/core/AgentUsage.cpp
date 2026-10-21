#include "AgentUsage.h"

#include <QtCore/QJsonObject>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace Syrtis {
namespace {
void require(bool condition)
{
    if (!condition) throw std::invalid_argument("invalid UsageWindow");
}

template<typename T> T scalar(const QJsonValue &value)
{
    if constexpr (std::is_same_v<T, QString>) {
        require(value.isString());
        return value.toString();
    } else if constexpr (std::is_same_v<T, bool>) {
        require(value.isBool());
        return value.toBool();
    } else if constexpr (std::is_same_v<T, double>) {
        require(value.isDouble() && std::isfinite(value.toDouble()));
        return value.toDouble();
    } else {
        require(value.isDouble());
        const auto sentinel = std::numeric_limits<qint64>::min();
        const auto integer = value.toInteger(sentinel);
        require(integer != sentinel || value.toDouble() == -0x1p63);
        return integer;
    }
}

template<typename T> std::optional<T> optional(const QJsonObject &object, const char *key)
{
    const auto value = object.value(QLatin1String(key));
    if (value.isUndefined() || value.isNull()) return std::nullopt;
    return scalar<T>(value);
}

template<typename T> T enumeration(const QString &raw, std::initializer_list<std::pair<const char *, T>> values)
{
    for (const auto &[name, value] : values)
        if (raw == QLatin1String(name)) return value;
    throw std::invalid_argument("invalid pace enum");
}

HistoricalPace decodeHistorical(const QJsonValue &value)
{
    require(value.isObject());
    const auto object = value.toObject();
    HistoricalPace result{scalar<double>(object["expectedUsedPercent"]),
        optional<double>(object, "etaSeconds"), scalar<bool>(object["willLastToReset"]),
        optional<double>(object, "runOutProbability")};
    require(result.expectedUsedPercent >= 0 && result.expectedUsedPercent <= 100);
    require(!result.etaSeconds || *result.etaSeconds >= 0);
    require(!result.runOutProbability || (*result.runOutProbability >= 0 && *result.runOutProbability <= 1));
    require(!result.etaSeconds == result.willLastToReset);
    return result;
}

PaceStatus decodeStatus(const QJsonValue &value)
{
    require(value.isObject());
    const auto object = value.toObject();
    PaceStatus result;
    using State = UsagePaceState;
    result.state = enumeration<State>(scalar<QString>(object["state"]), {
        {"learningDuration", State::learningDuration}, {"learningHistory", State::learningHistory},
        {"available", State::available}, {"unavailable", State::unavailable}});
    result.windowKey = optional<QString>(object, "windowKey");
    result.durationSeconds = optional<qint64>(object, "durationSeconds");
    if (const auto source = optional<QString>(object, "durationSource")) {
        using Source = UsagePaceDurationSource;
        result.durationSource = enumeration<Source>(*source, {
            {"provider", Source::provider}, {"contract", Source::contract}, {"observed", Source::observed}});
    }
    result.completeCycles = scalar<qint64>(object["completeCycles"]);
    if (const auto reason = optional<QString>(object, "reason")) {
        using Reason = UsagePaceUnavailableReason;
        result.reason = enumeration<Reason>(*reason, {
            {"windowIdentity", Reason::windowIdentity}, {"missingReset", Reason::missingReset},
            {"invalidEvidence", Reason::invalidEvidence}, {"accountScope", Reason::accountScope},
            {"storeCapacity", Reason::storeCapacity}, {"history", Reason::history},
            {"nonRecurring", Reason::nonRecurring}});
    }
    require(result.completeCycles >= 0);
    const bool identityUnavailable = result.state == State::unavailable
        && result.reason == UsagePaceUnavailableReason::windowIdentity;
    require(!result.windowKey == identityUnavailable);
    require(!result.windowKey || !result.windowKey->trimmed().isEmpty());
    if (result.durationSeconds) {
        require(*result.durationSeconds > 0 && *result.durationSeconds <= 400 * 86400);
        require(result.durationSource.has_value());
    } else if (result.durationSource) {
        require(result.state == State::learningDuration
                && result.durationSource == UsagePaceDurationSource::observed);
    }
    switch (result.state) {
    case State::learningDuration:
        require(!result.durationSeconds && !result.reason);
        break;
    case State::learningHistory:
    case State::available:
        require(result.durationSeconds && result.durationSource && !result.reason);
        break;
    case State::unavailable:
        require(result.reason.has_value());
        require(result.durationSeconds || !result.durationSource);
        break;
    case State::legacyMissing:
        require(false);
    }
    return result;
}
}

std::optional<UsageWindow> UsageWindow::decode(const QJsonValue &value)
{
    try {
        require(value.isObject());
        const auto object = value.toObject();
        UsageWindow result;
        result.label = scalar<QString>(object["label"]);
        result.usedPercent = scalar<double>(object["usedPercent"]);
        result.remainingPercent = scalar<double>(object["remainingPercent"]);
        require(result.usedPercent >= 0 && result.usedPercent <= 100
                && result.remainingPercent >= 0 && result.remainingPercent <= 100);
        require(std::abs(result.usedPercent + result.remainingPercent - 100) < 0.000001);
        result.resetsAt = optional<QString>(object, "resetsAt");
        result.resetText = optional<QString>(object, "resetText");
        result.windowMinutes = optional<qint64>(object, "windowMinutes");
        result.modelScope = optional<QString>(object, "modelScope");
        if (const auto historical = object["historicalPace"]; !historical.isUndefined() && !historical.isNull())
            result.historicalPace = decodeHistorical(historical);
        const bool v3 = object.contains("paceStatus");
        if (v3 || object.contains("cardId")) {
            result.cardId = scalar<QString>(object["cardId"]);
            require(!result.cardId.trimmed().isEmpty());
        }
        if (!v3) return result;
        result.paceStatus = decodeStatus(object["paceStatus"]);
        result.durationSeconds = result.paceStatus.durationSeconds;
        // Top-level durationSeconds is deliberately not a Swift CodingKey.
        if (result.durationSeconds)
            require(result.windowMinutes == *result.durationSeconds / 60);
        else
            require(!result.windowMinutes);
        using State = UsagePaceState;
        switch (result.paceStatus.state) {
        case State::available:
            require(result.durationSeconds && result.historicalPace);
            break;
        case State::learningHistory:
            require(result.durationSeconds && !result.historicalPace);
            break;
        case State::learningDuration:
            require(!result.durationSeconds && !result.historicalPace);
            break;
        case State::unavailable:
            require(!result.historicalPace);
            break;
        case State::legacyMissing:
            require(false);
        }
        return result;
    } catch (const std::invalid_argument &) {
        return std::nullopt;
    }
}
}
