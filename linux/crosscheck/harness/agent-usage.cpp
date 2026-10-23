#include "agent-usage.h"
#include "AgentUsage.h"
#include "QuotaCurve.h"
#include "TBCore.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QSet>
#include <QtCore/QTimeZone>

using namespace syrtis::core;
namespace {
QJsonValue wire(const QString &v) { return v; }
QJsonValue wire(bool v) { return v; }
QJsonValue wire(double v) { return v; }
QJsonValue wire(qint64 v) { return v; }
QJsonValue wire(quint64 v) {
    return v <= quint64(std::numeric_limits<qint64>::max()) ? QJsonValue(qint64(v)) : QJsonValue(double(v));
}
QJsonValue wire(UsagePaceState v) {
    switch (v) {
    case UsagePaceState::learningDuration: return "learningDuration";
    case UsagePaceState::learningHistory: return "learningHistory";
    case UsagePaceState::available: return "available";
    case UsagePaceState::unavailable: return "unavailable";
    case UsagePaceState::legacyMissing: return "legacyMissing";
    }
    throw std::runtime_error("invalid enum");
}
QJsonValue wire(UsagePaceDurationSource v) {
    switch (v) {
    case UsagePaceDurationSource::provider: return "provider";
    case UsagePaceDurationSource::contract: return "contract";
    case UsagePaceDurationSource::observed: return "observed";
    }
    throw std::runtime_error("invalid enum");
}
QJsonValue wire(UsagePaceUnavailableReason v) {
    switch (v) {
    case UsagePaceUnavailableReason::windowIdentity: return "windowIdentity";
    case UsagePaceUnavailableReason::missingReset: return "missingReset";
    case UsagePaceUnavailableReason::invalidEvidence: return "invalidEvidence";
    case UsagePaceUnavailableReason::accountScope: return "accountScope";
    case UsagePaceUnavailableReason::storeCapacity: return "storeCapacity";
    case UsagePaceUnavailableReason::history: return "history";
    case UsagePaceUnavailableReason::nonRecurring: return "nonRecurring";
    }
    throw std::runtime_error("invalid enum");
}
QJsonValue wire(QuotaCurveDurationSource v) {
    switch (v) {
    case QuotaCurveDurationSource::provider: return "provider";
    case QuotaCurveDurationSource::contract: return "contract";
    case QuotaCurveDurationSource::observed: return "observed";
    }
    throw std::runtime_error("invalid enum");
}
QJsonValue wire(QuotaCurveSampleOrigin v) {
    switch (v) {
    case QuotaCurveSampleOrigin::liveV3: return "liveV3";
    case QuotaCurveSampleOrigin::importedV2: return "importedV2";
    }
    throw std::runtime_error("invalid enum");
}
QJsonValue wire(const HistoricalPace &v);
QJsonValue wire(const PaceStatus &v);
QJsonValue wire(const UsageWindow &v);
QJsonValue wire(const AgentIdentity &v);
QJsonValue wire(const CreditsSnapshot &v);
QJsonValue wire(const AgentUsageTransportDiagnostic &v);
QJsonValue wire(const AgentUsageSnapshot &v);
QJsonValue wire(const AgentUsagePayload &v);
QJsonValue wire(const QuotaProviderIds &v);
QJsonValue wire(const QuotaCurvePoint &v);
QJsonValue wire(const QuotaCurveCoverage &v);
QJsonValue wire(const QuotaCurve &v);
template<class T> QJsonValue wire(const QList<T> &v);
template<class T> QJsonValue wire(const std::optional<T> &v) { return v ? wire(*v) : QJsonValue(QJsonValue::Null); }
template<class T> QJsonValue wire(const QList<T> &v) {
    QJsonArray out;
    for (const auto &item : v) out.append(wire(item));
    return out;
}
QJsonValue wire(const HistoricalPace &v) {
    return QJsonObject{{"expectedUsedPercent", wire(v.expectedUsedPercent)}, {"etaSeconds", wire(v.etaSeconds)}, {"willLastToReset", wire(v.willLastToReset)}, {"runOutProbability", wire(v.runOutProbability)}};
}
QJsonValue wire(const PaceStatus &v) {
    return QJsonObject{{"state", wire(v.state)}, {"windowKey", wire(v.windowKey)}, {"durationSeconds", wire(v.durationSeconds)}, {"durationSource", wire(v.durationSource)}, {"completeCycles", wire(v.completeCycles)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const UsageWindow &v) {
    return QJsonObject{{"cardId", wire(v.cardId)}, {"label", wire(v.label)}, {"usedPercent", wire(v.usedPercent)}, {"remainingPercent", wire(v.remainingPercent)}, {"resetsAt", wire(v.resetsAt)}, {"resetText", wire(v.resetText)}, {"windowMinutes", wire(v.windowMinutes)}, {"durationSeconds", wire(v.durationSeconds)}, {"paceStatus", wire(v.paceStatus)}, {"historicalPace", wire(v.historicalPace)}, {"modelScope", wire(v.modelScope)}};
}
QJsonValue wire(const AgentIdentity &v) {
    return QJsonObject{{"email", wire(v.email)}, {"plan", wire(v.plan)}};
}
QJsonValue wire(const CreditsSnapshot &v) {
    return QJsonObject{{"remaining", wire(v.remaining)}, {"unlimited", wire(v.unlimited)}};
}
QJsonValue wire(const AgentUsageTransportDiagnostic &v) {
    return QJsonObject{{"category", wire(v.category)}, {"status", wire(v.status)}, {"osCode", wire(v.osCode)}};
}
QJsonValue wire(const AgentUsageSnapshot &v) {
    return QJsonObject{{"clientId", wire(v.clientId)}, {"accountKey", wire(v.accountKey)}, {"source", wire(v.source)}, {"updatedAt", wire(v.updatedAt)}, {"identity", wire(v.identity)}, {"windows", wire(v.windows)}, {"credits", wire(v.credits)}, {"error", wire(v.error)}, {"transportDiagnostic", wire(v.transportDiagnostic)}, {"agyLoginMarker", wire(v.agyLoginMarker)}, {"boundAccountKey", wire(v.boundAccountKey)}, {"historyAccountKey", wire(v.historyAccountKey)}};
}
QJsonValue wire(const AgentUsagePayload &v) {
    return QJsonObject{{"generatedAt", wire(v.generatedAt)}, {"publicationGeneration", wire(v.publicationGeneration)}, {"agents", wire(v.agents)}, {"opencodeSubscriptions", wire(v.opencodeSubscriptions)}};
}
QJsonValue wire(const QuotaProviderIds &v) {
    return QJsonObject{{"ids", wire(v.ids)}};
}
QJsonValue wire(const QuotaCurvePoint &v) {
    return QJsonObject{{"sampledAt", wire(v.sampledAt)}, {"usedPercent", wire(v.usedPercent)}, {"resetAt", wire(v.resetAt)}, {"durationSeconds", wire(v.durationSeconds)}, {"durationSource", wire(v.durationSource)}, {"origin", wire(v.origin)}, {"isActiveGroup", wire(v.isActiveGroup)}};
}
QJsonValue wire(const QuotaCurveCoverage &v) {
    return QJsonObject{{"oldestSampledAt", wire(v.oldestSampledAt)}, {"newestSampledAt", wire(v.newestSampledAt)}, {"sampleCount", wire(v.sampleCount)}};
}
QJsonValue wire(const QuotaCurve &v) {
    return QJsonObject{{"points", wire(v.points)}, {"coverage", wire(v.coverage)}, {"activeResetAt", wire(v.activeResetAt)}, {"generation", wire(v.generation)}};
}
QJsonArray names(const QList<UsageWindow> &windows) {
    QJsonArray out;
    for (const auto &w : windows) out.append(QJsonObject{{"cardId", w.cardId}, {"label", w.label}});
    return out;
}
QJsonObject card(const AgentUsageSnapshot &v) {
    const auto setup = v.setupInstructions();
    const auto kind = setup.kind == SetupInstructions::Kind::claudeSetupToken ? "claudeSetupToken"
        : setup.kind == SetupInstructions::Kind::providerMessage ? "providerMessage" : "none";
    QJsonObject instructions{{"kind", kind}};
    if (setup.kind == SetupInstructions::Kind::providerMessage) instructions["message"] = setup.message;
    return {{"isSetupPlaceholder", v.isSetupPlaceholder()}, {"setupBadgeKey", wire(v.setupBadgeKey())},
        {"setupInstructions", instructions}, {"historyReadAccountKey", wire(v.historyReadAccountKey())},
        {"rawCardWindows", wire(v.rawCardWindows())}};
}
template<class T> QJsonObject run(const QJsonObject &c) {
    const auto input = c.contains("rawJSON") ? json::parse(c["rawJSON"].toString().toUtf8()) : c["input"];
    const auto mode = c["mode"].toString("bare");
    if (mode == "optional") return {{"value", wire(TBCore::decodeOptionalEnvelope<T>(input))}};
    const auto v = mode == "envelope" ? TBCore::decodeEnvelope<T>(input) : TBCore::decode<T>(input);
    QJsonObject out{{"value", wire(v)}};
    if constexpr (std::is_same_v<T, AgentUsagePayload>) out["configuredClientIds"] = wire(v.configuredClientIds());
    if constexpr (std::is_same_v<T, AgentUsageSnapshot>) out["card"] = card(v);
    if constexpr (std::is_same_v<T, PaceStatus>) out["historyKey"] = wire(v.historyKey());
    return out;
}
QJsonObject decodeCase(const QJsonObject &c) {
    const auto op = c["op"].toString("decode");
    if (op == "qualify") {
        auto input = c["input"].toObject();
        auto windows = input["windows"].toArray();
        // Only names are emitted here. Relative reset inputs exercise shipping
        // countdown formatting without putting wall-clock timestamps in goldens.
        const auto offsets = c["resetOffsetsSeconds"].toArray();
        constexpr double now = 0; // Foundation reference date.
        for (qsizetype i = 0; i < offsets.size(); ++i) {
            if (offsets[i].isNull()) continue;
            auto w = windows[i].toObject();
            w["resetsAt"] = QDateTime::fromMSecsSinceEpoch(qint64((978307200.0 + offsets[i].toDouble()) * 1000), QTimeZone::UTC).toString(Qt::ISODateWithMs);
            windows[i] = w;
        }
        input["windows"] = windows;
        const auto v = TBCore::decode<AgentUsageSnapshot>(QJsonValue(input));
        return {{"value", QJsonObject{{"raw", names(v.rawCardWindows())},
            {"qualified", names(AgentUsageSnapshot::qualifyingRepeatedLabels(v.rawCardWindows(), now))}}}};
    }
    if (op == "adoptHistory") {
        const auto primary = TBCore::decode<AgentUsageSnapshot>(c["input"]);
        const auto result = primary.adoptingHistory(TBCore::decode<AgentUsageSnapshot>(c["other"]));
        return {{"value", wire(result)}, {"card", card(result)}};
    }
    if (op == "replacePace") {
        const auto result = TBCore::decode<UsageWindow>(c["input"]).replacingPace(TBCore::decode<UsageWindow>(c["other"]));
        return {{"value", wire(result)}};
    }
    if (op == "replaceIdentity") {
        const auto result = TBCore::decode<AgentUsageSnapshot>(c["input"]).replacingIdentity(TBCore::decode<std::optional<AgentIdentity>>(c["other"]));
        return {{"value", wire(result)}, {"card", card(result)}};
    }
    if (op == "replaceAgents") {
        const auto result = TBCore::decode<AgentUsagePayload>(c["input"]).replacingAgents(TBCore::decode<QList<AgentUsageSnapshot>>(c["other"]));
        return {{"value", wire(result)}, {"configuredClientIds", wire(result.configuredClientIds())}};
    }
    if (op != "decode") throw std::runtime_error("unknown agent-usage operation");
    const auto type = c["type"].toString();
    if (type == "HistoricalPace") return run<HistoricalPace>(c);
    if (type == "PaceStatus") return run<PaceStatus>(c);
    if (type == "UsageWindow") return run<UsageWindow>(c);
    if (type == "AgentIdentity") return run<AgentIdentity>(c);
    if (type == "CreditsSnapshot") return run<CreditsSnapshot>(c);
    if (type == "AgentUsageTransportDiagnostic") return run<AgentUsageTransportDiagnostic>(c);
    if (type == "AgentUsageSnapshot") return run<AgentUsageSnapshot>(c);
    if (type == "AgentUsagePayload") return run<AgentUsagePayload>(c);
    if (type == "QuotaProviderIds") return run<QuotaProviderIds>(c);
    if (type == "QuotaCurvePoint") return run<QuotaCurvePoint>(c);
    if (type == "QuotaCurveCoverage") return run<QuotaCurveCoverage>(c);
    if (type == "QuotaCurve") return run<QuotaCurve>(c);
    throw std::runtime_error("unknown agent-usage fixture type");
}
}
QByteArray agentUsage(const QString &fixturesDir)
{
    QFile file(QDir(fixturesDir).filePath("agent-usage.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read agent-usage fixture");
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() || !doc.object()["cases"].isArray())
        throw std::runtime_error("invalid agent-usage fixture cases");
    QJsonArray out;
    QSet<QString> ids;
    for (const auto &v : doc.object()["cases"].toArray()) {
        const auto c = v.toObject();
        if (!v.isObject() || !c["id"].isString() || ids.contains(c["id"].toString()) || !c["type"].isString()
            || (!c.contains("input") && !c["rawJSON"].isString())
            || (c.contains("mode") && !QStringList{"bare", "envelope", "optional"}.contains(c["mode"].toString()))
            || (c.contains("resetOffsetsSeconds") && (!c["resetOffsetsSeconds"].isArray()
                || c["resetOffsetsSeconds"].toArray().size() != c["input"].toObject()["windows"].toArray().size())))
            throw std::runtime_error("invalid agent-usage fixture case");
        ids.insert(c["id"].toString());
        QJsonObject result;
        try { result = decodeCase(c); }
        catch (const TBCoreError &e) { result["error"] = QJsonObject{{"kind", "bridge"}, {"message", e.message}}; }
        catch (const DecodingError &e) {
            QJsonObject detail{{"kind", e.kind}, {"path", wire(e.path)}};
            if (e.kind == "keyNotFound") detail["field"] = e.field;
            else if (!e.field.isEmpty()) detail["description"] = e.field;
            result["error"] = detail;
        }
        result["id"] = c["id"];
        out.append(result);
    }
    return QJsonDocument(QJsonObject{{"cases", out}}).toJson();
}
