#include "reports.h"
#include "TBCore.h"
#include "AgentsReport.h"
#include "HourlyReport.h"
#include "ModelReport.h"
#include "FilterParityProbe.h"
#include <QtCore/QDir>
#include <QtCore/QFile>

using namespace syrtis::core;
namespace {
QJsonValue wire(const QString &v) { return v; }
QJsonValue wire(bool v) { return v; }
QJsonValue wire(qint64 v) { return v; }
QJsonValue wire(quint64 v) {
    return v <= quint64(std::numeric_limits<qint64>::max())
        ? QJsonValue(qint64(v)) : QJsonValue(double(v));
}
QJsonValue wire(double v) { return v; }
QJsonValue wire(FilterParityStatus v) {
    switch (v) {
    case FilterParityStatus::match: return "match";
    case FilterParityStatus::mismatch: return "mismatch";
    case FilterParityStatus::sourceChanged: return "sourceChanged";
    case FilterParityStatus::tokenUnavailable: return "tokenUnavailable";
    }
    throw std::runtime_error("invalid parity status");
}
QJsonValue wire(const ProbeResult &v);
QJsonValue wire(const ScanPathNote &v);
QJsonValue wire(const ExtraScanPathsResult &v);
QJsonValue wire(const RejectedConfigDir &v);
QJsonValue wire(const ClaudeConfigDirsResult &v);
QJsonValue wire(const RejectedAntigravityAccount &v);
QJsonValue wire(const AntigravityAccountsResult &v);
QJsonValue wire(const AntigravityCapturedAccount &v);
QJsonValue wire(const AntigravityBindingResult &v);
QJsonValue wire(const AntigravityRemoved &v);
QJsonValue wire(const AntigravityLoginMarker &v);
QJsonValue wire(const AntigravityAutoCaptureResult &v);
QJsonValue wire(const RejectedKeychainConsent &v);
QJsonValue wire(const KeychainConsentResult &v);
QJsonValue wire(const CursorSyncConfigResult &v);
QJsonValue wire(const CursorSyncStatus &v);
QJsonValue wire(const AgentReportEntry &v);
QJsonValue wire(const AgentsReport &v);
QJsonValue wire(const HourlyReportEntry &v);
QJsonValue wire(const HourlyReport &v);
QJsonValue wire(const ModelReportEntry &v);
QJsonValue wire(const ModelReport &v);
QJsonValue wire(const FilterParityAggregate &v);
QJsonValue wire(const FilterParityReport &v);
QJsonValue wire(const FilterParityProbe &v);
template<class T> QJsonValue wire(const std::optional<T> &v) { return v ? wire(*v) : QJsonValue(QJsonValue::Null); }
template<class T> QJsonValue wire(const QList<T> &v) {
    QJsonArray out;
    for (const auto &item : v) out.append(wire(item));
    return out;
}
QJsonValue wire(const ProbeResult &v) {
    return QJsonObject{{"ok", wire(v.ok)}, {"messages", wire(v.messages)}, {"err", wire(v.err)}};
}
QJsonValue wire(const ScanPathNote &v) {
    return QJsonObject{{"client", wire(v.client)}, {"path", wire(v.path)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const ExtraScanPathsResult &v) {
    return QJsonObject{{"registeredCount", wire(v.registeredCount)}, {"unreadable", wire(v.unreadable)}, {"rejected", wire(v.rejected)}};
}
QJsonValue wire(const RejectedConfigDir &v) {
    return QJsonObject{{"path", wire(v.path)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const ClaudeConfigDirsResult &v) {
    return QJsonObject{{"registeredCount", wire(v.registeredCount)}, {"rejected", wire(v.rejected)}};
}
QJsonValue wire(const RejectedAntigravityAccount &v) {
    return QJsonObject{{"index", wire(v.index)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const AntigravityAccountsResult &v) {
    return QJsonObject{{"registeredCount", wire(v.registeredCount)}, {"rejected", wire(v.rejected)}};
}
QJsonValue wire(const AntigravityCapturedAccount &v) {
    return QJsonObject{{"key", wire(v.key)}, {"label", wire(v.label)}};
}
QJsonValue wire(const AntigravityBindingResult &v) {
    return QJsonObject{{"bound", wire(v.bound)}};
}
QJsonValue wire(const AntigravityRemoved &v) {
    return QJsonObject{{"removed", wire(v.removed)}};
}
QJsonValue wire(const AntigravityLoginMarker &v) {
    return QJsonObject{{"marker", wire(v.marker)}};
}
QJsonValue wire(const AntigravityAutoCaptureResult &v) {
    return QJsonObject{{"status", wire(v.status)}, {"key", wire(v.key)}, {"label", wire(v.label)}};
}
QJsonValue wire(const RejectedKeychainConsent &v) {
    return QJsonObject{{"client", wire(v.client)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const KeychainConsentResult &v) {
    return QJsonObject{{"grantedCount", wire(v.grantedCount)}, {"rejected", wire(v.rejected)}};
}
QJsonValue wire(const CursorSyncConfigResult &v) {
    return QJsonObject{{"enabled", wire(v.enabled)}, {"dir", wire(v.dir)}, {"cliTakeoverConfirmed", wire(v.cliTakeoverConfirmed)}, {"removedFiles", wire(v.removedFiles)}, {"cleanupFailed", wire(v.cleanupFailed)}};
}
QJsonValue wire(const CursorSyncStatus &v) {
    return QJsonObject{{"state", wire(v.state)}, {"events", wire(v.events)}, {"lastSuccessMs", wire(v.lastSuccessMs)}, {"reason", wire(v.reason)}};
}
QJsonValue wire(const AgentReportEntry &v) {
    return QJsonObject{{"agent", wire(v.agent)}, {"clients", wire(v.clients)}, {"input", wire(v.input)}, {"output", wire(v.output)}, {"cacheRead", wire(v.cacheRead)}, {"cacheWrite", wire(v.cacheWrite)}, {"reasoning", wire(v.reasoning)}, {"total", wire(v.total)}, {"cost", wire(v.cost)}, {"messages", wire(v.messages)}};
}
QJsonValue wire(const AgentsReport &v) {
    return QJsonObject{{"entries", wire(v.entries)}, {"totalCost", wire(v.totalCost)}, {"totalMessages", wire(v.totalMessages)}};
}
QJsonValue wire(const HourlyReportEntry &v) {
    return QJsonObject{{"hour", wire(v.hour)}, {"clients", wire(v.clients)}, {"models", wire(v.models)}, {"input", wire(v.input)}, {"output", wire(v.output)}, {"cacheRead", wire(v.cacheRead)}, {"cacheWrite", wire(v.cacheWrite)}, {"reasoning", wire(v.reasoning)}, {"total", wire(v.total)}, {"messageCount", wire(v.messageCount)}, {"turnCount", wire(v.turnCount)}, {"cost", wire(v.cost)}};
}
QJsonValue wire(const HourlyReport &v) {
    return QJsonObject{{"entries", wire(v.entries)}, {"totalCost", wire(v.totalCost)}};
}
QJsonValue wire(const ModelReportEntry &v) {
    return QJsonObject{{"client", wire(v.client)}, {"model", wire(v.model)}, {"provider", wire(v.provider)}, {"input", wire(v.input)}, {"output", wire(v.output)}, {"cacheRead", wire(v.cacheRead)}, {"cacheWrite", wire(v.cacheWrite)}, {"reasoning", wire(v.reasoning)}, {"total", wire(v.total)}, {"messageCount", wire(v.messageCount)}, {"cost", wire(v.cost)}, {"msPer1kTokens", wire(v.msPer1kTokens)}, {"costEstimate", wire(v.costEstimate)}};
}
QJsonValue wire(const ModelReport &v) {
    return QJsonObject{{"entries", wire(v.entries)}, {"totalInput", wire(v.totalInput)}, {"totalOutput", wire(v.totalOutput)}, {"totalCacheRead", wire(v.totalCacheRead)}, {"totalCacheWrite", wire(v.totalCacheWrite)}, {"totalMessages", wire(v.totalMessages)}, {"totalCost", wire(v.totalCost)}, {"pricingUpdatedAt", wire(v.pricingUpdatedAt)}};
}
QJsonValue wire(const FilterParityAggregate &v) {
    return QJsonObject{{"entryCount", wire(v.entryCount)}, {"input", wire(v.input)}, {"output", wire(v.output)}, {"cacheRead", wire(v.cacheRead)}, {"cacheWrite", wire(v.cacheWrite)}, {"reasoning", wire(v.reasoning)}, {"totalTokens", wire(v.totalTokens)}, {"messageCount", wire(v.messageCount)}, {"totalCost", wire(v.totalCost)}};
}
QJsonValue wire(const FilterParityReport &v) {
    return QJsonObject{{"status", wire(v.status)}, {"unfiltered", wire(v.unfiltered)}, {"full", wire(v.full)}, {"delta", wire(v.delta)}};
}
QJsonValue wire(const FilterParityProbe &v) {
    return QJsonObject{{"hourly", wire(v.hourly)}, {"agents", wire(v.agents)}, {"presentClientCount", wire(v.presentClientCount)}};
}
template<class T> QJsonObject run(const QJsonObject &c) {
    const auto mode = c["mode"].toString("envelope");
    const auto input = c.contains("rawJSON") ? json::parse(c["rawJSON"].toString().toUtf8()) : c["input"];
    if (mode == "optional") return {{"value", wire(TBCore::decodeOptionalEnvelope<T>(input))}};
    const auto value = mode == "bare" ? TBCore::decode<T>(input) : TBCore::decodeEnvelope<T>(input);
    QJsonObject out{{"value", wire(value)}};
    if constexpr (std::is_same_v<T, FilterParityProbe>)
        out["smokeSummary"] = value.smokeSummary();
    return out;
}
QJsonObject decodeCase(const QJsonObject &c) {
    const auto type = c["type"].toString();
    if (type == "ProbeResult") return run<ProbeResult>(c);
    if (type == "ScanPathNote") return run<ScanPathNote>(c);
    if (type == "ExtraScanPathsResult") return run<ExtraScanPathsResult>(c);
    if (type == "RejectedConfigDir") return run<RejectedConfigDir>(c);
    if (type == "ClaudeConfigDirsResult") return run<ClaudeConfigDirsResult>(c);
    if (type == "RejectedAntigravityAccount") return run<RejectedAntigravityAccount>(c);
    if (type == "AntigravityAccountsResult") return run<AntigravityAccountsResult>(c);
    if (type == "AntigravityCapturedAccount") return run<AntigravityCapturedAccount>(c);
    if (type == "AntigravityBindingResult") return run<AntigravityBindingResult>(c);
    if (type == "AntigravityRemoved") return run<AntigravityRemoved>(c);
    if (type == "AntigravityLoginMarker") return run<AntigravityLoginMarker>(c);
    if (type == "AntigravityAutoCaptureResult") return run<AntigravityAutoCaptureResult>(c);
    if (type == "RejectedKeychainConsent") return run<RejectedKeychainConsent>(c);
    if (type == "KeychainConsentResult") return run<KeychainConsentResult>(c);
    if (type == "CursorSyncConfigResult") return run<CursorSyncConfigResult>(c);
    if (type == "CursorSyncStatus") return run<CursorSyncStatus>(c);
    if (type == "AgentReportEntry") return run<AgentReportEntry>(c);
    if (type == "AgentsReport") return run<AgentsReport>(c);
    if (type == "HourlyReportEntry") return run<HourlyReportEntry>(c);
    if (type == "HourlyReport") return run<HourlyReport>(c);
    if (type == "ModelReportEntry") return run<ModelReportEntry>(c);
    if (type == "ModelReport") return run<ModelReport>(c);
    if (type == "FilterParityAggregate") return run<FilterParityAggregate>(c);
    if (type == "FilterParityReport") return run<FilterParityReport>(c);
    if (type == "FilterParityProbe") return run<FilterParityProbe>(c);
    throw std::runtime_error("unknown reports fixture type");
}
}
QByteArray reports(const QString &fixturesDir)
{
    QFile file(QDir(fixturesDir).filePath("reports.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read reports fixture");
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || !doc.object()["cases"].isArray()) throw std::runtime_error("invalid reports fixture cases");
    QJsonArray out;
    for (const auto &v : doc.object()["cases"].toArray()) {
        const auto c = v.toObject();
        if (!v.isObject() || !c["id"].isString() || !c["type"].isString()
            || (!c.contains("input") && !c["rawJSON"].isString())
            || (c.contains("mode") && !QStringList{"bare", "envelope", "optional"}.contains(c["mode"].toString())))
            throw std::runtime_error("invalid reports fixture case");
        QJsonObject result;
        try {
            result = decodeCase(c);
        } catch (const TBCoreError &e) {
            result["error"] = QJsonObject{{"kind", "bridge"}, {"message", e.message}};
        } catch (const DecodingError &e) {
            QJsonObject error{{"kind", e.kind}, {"path", wire(e.path)}};
            if (e.kind == "keyNotFound") error["field"] = e.field;
            else if (!e.field.isEmpty()) error["description"] = e.field;
            result["error"] = error;
        }
        result["id"] = c["id"];
        out.append(result);
    }
    return QJsonDocument(QJsonObject{{"cases", out}}).toJson();
}
