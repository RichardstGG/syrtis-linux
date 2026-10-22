#include "provider-quota-pace-v3.h"
#include "QuotaResolver.h"
#include "TBCore.h"
#include "UsagePace.h"
#include <QtCore/QDir>
#include <QtCore/QFile>

using namespace Syrtis;
using namespace syrtis::core;
namespace {
struct ProviderCase {
    QJsonObject value;
    static ProviderCase decode(const json::Decoder &d) {
        d.object();
        d.field<QString>("name");
        d.field<QString>("kind");
        for (const auto &key : {"clientId", "cardId", "mode", "now", "selection", "rawWindow"})
            d.field<std::optional<QString>>(key);
        return {d.value.toObject()};
    }
};
template<class T> QJsonValue optional(const std::optional<T> &v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}
QString stateName(UsagePaceState state)
{
    switch (state) {
    case UsagePaceState::learningDuration: return "learningDuration";
    case UsagePaceState::learningHistory: return "learningHistory";
    case UsagePaceState::available: return "available";
    case UsagePaceState::unavailable: return "unavailable";
    case UsagePaceState::legacyMissing: return "legacyMissing";
    }
    throw std::logic_error("unknown pace state");
}
QJsonValue durationSource(const std::optional<UsagePaceDurationSource> &source)
{
    if (!source) return QJsonValue::Null;
    switch (*source) {
    case UsagePaceDurationSource::provider: return "provider";
    case UsagePaceDurationSource::contract: return "contract";
    case UsagePaceDurationSource::observed: return "observed";
    }
    throw std::logic_error("unknown duration source");
}
QJsonValue reasonName(const std::optional<UsagePaceUnavailableReason> &reason)
{
    if (!reason) return QJsonValue::Null;
    switch (*reason) {
    case UsagePaceUnavailableReason::windowIdentity: return "windowIdentity";
    case UsagePaceUnavailableReason::missingReset: return "missingReset";
    case UsagePaceUnavailableReason::invalidEvidence: return "invalidEvidence";
    case UsagePaceUnavailableReason::accountScope: return "accountScope";
    case UsagePaceUnavailableReason::storeCapacity: return "storeCapacity";
    case UsagePaceUnavailableReason::history: return "history";
    case UsagePaceUnavailableReason::nonRecurring: return "nonRecurring";
    }
    throw std::logic_error("unknown unavailable reason");
}
QString stageName(PaceStage stage)
{
    switch (stage) {
    case PaceStage::onTrack: return "onTrack";
    case PaceStage::slightlyAhead: return "slightlyAhead";
    case PaceStage::ahead: return "ahead";
    case PaceStage::farAhead: return "farAhead";
    case PaceStage::slightlyBehind: return "slightlyBehind";
    case PaceStage::behind: return "behind";
    case PaceStage::farBehind: return "farBehind";
    }
    throw std::logic_error("unknown pace stage");
}
QJsonObject lifecycle(const UsageWindow &window)
{
    return {{"state", stateName(window.paceStatus.state)}, {"reason", reasonName(window.paceStatus.reason)},
        {"durationSeconds", optional(window.paceStatus.durationSeconds)},
        {"durationSource", durationSource(window.paceStatus.durationSource)},
        {"completeCycles", window.paceStatus.completeCycles}};
}
QJsonValue runCase(const QJsonObject &c, const AgentUsagePayload &payload)
{
    const auto kind = c["kind"].toString();
    if (kind == "selection") {
        if (!c["selection"].isString()) return QJsonObject{{"error", "selection case needs selection"}};
        const auto selection = c["selection"].toString();
        const auto resolved = QuotaResolver::resolve(payload, selection);
        return QJsonObject{{"selection", selection},
            {"canonicalSelection", QuotaResolver::canonicalSelection(payload, selection)},
            {"resolvedClientId", resolved ? QJsonValue(resolved->clientId) : QJsonValue::Null},
            {"resolvedCardId", resolved ? QJsonValue(resolved->window.cardId) : QJsonValue::Null}};
    }
    if (kind == "pace") {
        const auto rawMode = c["mode"].toString();
        std::optional<PaceMode> mode;
        if (rawMode == "historical") mode = PaceMode::historical;
        if (rawMode == "linear") mode = PaceMode::linear;
        if (rawMode == "off") mode = PaceMode::off;
        const auto now = parseRFC3339(c["now"].toString());
        std::optional<UsageWindow> window;
        for (const auto &agent : payload.agents) {
            if (agent.clientId != c["clientId"].toString()) continue;
            for (const auto &w : agent.uniqueCardWindows())
                if (w.cardId == c["cardId"].toString()) { window = w; break; }
            break;
        }
        if (!mode || !now || !window) return QJsonObject{{"error", "invalid pace case metadata"}};
        const auto pace = UsagePace::compute(*window, *mode, *now);
        if (!pace) return QJsonValue::Null;
        const auto shown = UsagePace::presentation(*window, *mode, *pace);
        return QJsonObject{{"basis", pace->basis == UsagePaceBasis::linear ? "linear" : "historical"},
            {"stage", stageName(pace->stage)}, {"deltaPercent", pace->deltaPercent},
            {"expectedUsedPercent", pace->expectedUsedPercent}, {"actualUsedPercent", pace->actualUsedPercent},
            {"etaSeconds", optional(pace->etaSeconds)}, {"willLastToReset", pace->willLastToReset},
            {"label", pace->label()}, {"etaText", optional(shown.etaText)}, {"riskText", optional(shown.riskText)},
            {"isHistoricalDeficit", pace->isHistoricalDeficit()}};
    }
    if (kind == "legacy" || kind == "malformed") {
        const auto now = parseRFC3339(c["now"].toString());
        if (!c["rawWindow"].isString() || (kind == "legacy" && !now))
            return QJsonObject{{"error", kind == "legacy" ? "legacy case needs rawWindow and now" : "malformed case needs rawWindow"}};
        try {
            const auto window = TBCore::decode<UsageWindow>(c["rawWindow"].toString().toUtf8());
            if (kind == "malformed") return QJsonObject{{"rejected", false}};
            auto result = lifecycle(window);
            result["rejected"] = false;
            result["windowMinutes"] = optional(window.windowMinutes);
            result["historicalPace"] = UsagePace::compute(window, PaceMode::historical, *now).has_value();
            result["linearPace"] = UsagePace::compute(window, PaceMode::linear, *now).has_value();
            return result;
        } catch (const DecodingError &) { return QJsonObject{{"rejected", true}}; }
    }
    return QJsonObject{{"error", "unknown kind " + kind}};
}
}

QByteArray providerQuotaPaceV3(const QString &fixturesDir)
{
    QFile file(QDir(fixturesDir).filePath("provider-quota-pace-v3.json"));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read provider-quota-pace-v3 fixture");
    const json::Decoder root{json::parse(file.readAll()), {}};
    const auto version = root.field<qint64>("schemaVersion");
    if (version != 3) throw std::runtime_error("provider-quota-pace-v3 schemaVersion must be 3");
    const auto payload = root.field<AgentUsagePayload>("payload");
    // All wire values enter through the same typed decoder as production.
    const auto cases = root.field<QList<ProviderCase>>("cases");
    QJsonArray rows;
    for (const auto &agent : payload.agents) {
        for (const auto &window : agent.windows) {
            auto row = lifecycle(window);
            row["clientId"] = agent.clientId;
            row["cardId"] = window.cardId;
            row["label"] = window.label;
            row["hasHistorical"] = window.historicalPace.has_value();
            rows.append(row);
        }
    }
    QJsonObject output;
    for (const auto &item : cases) {
        const auto &c = item.value;
        const auto name = c["name"].toString();
        if (output.contains(name)) throw std::runtime_error("duplicate provider case name");
        output[name] = runCase(c, payload);
    }
    return QJsonDocument(QJsonObject{{"schemaVersion", version}, {"lifecycle", rows}, {"cases", output}}).toJson();
}
