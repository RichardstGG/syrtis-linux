#include "QuotaOverview.h"
#include "QuotaSummary.h"
#include "TBCore.h"
#include <QtCore/QSet>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Syrtis;
using namespace syrtis::core;
namespace {
void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
AgentUsagePayload sample()
{
    return TBCore::decode<AgentUsagePayload>(QByteArray(R"({
      "generatedAt":"2026-07-10T12:00:00Z","agents":[
        {"clientId":"claude","source":"synthetic","updatedAt":"synthetic","windows":[
          {"cardId":"session.v1","label":"Shared","usedPercent":72,"remainingPercent":28,
           "resetsAt":"2026-07-10T15:00:00Z","windowMinutes":300,
           "paceStatus":{"state":"available","windowKey":"primary","durationSeconds":18000,
                         "durationSource":"provider","completeCycles":0},
           "historicalPace":{"expectedUsedPercent":32,"etaSeconds":3600,"willLastToReset":false,"runOutProbability":0.75}}]},
        {"clientId":"claude","accountKey":"/synthetic/work","source":"synthetic","updatedAt":"synthetic","windows":[
          {"cardId":"session.v1","label":"Shared","usedPercent":20,"remainingPercent":80}]},
        {"clientId":"codex","source":"synthetic","updatedAt":"synthetic","windows":[
          {"cardId":"weekly","label":"Weekly","usedPercent":40,"remainingPercent":60}]}
      ]})"));
}
}
int main()
{
    try {
        const AccountIdentity primary{"claude", std::nullopt}, extra{"claude", "/synthetic/work/"};
        check(primary.isPrimary() && primary.hasLocalUsage() && !primary.accountLabel() && !primary.accountTooltip(), "primary identity");
        check(primary.windowKey("session.v1") == "claude|session.v1", "primary window key");
        check(!extra.isPrimary() && extra.hasLocalUsage() && extra.accountLabel() == "work"
            && extra.accountTooltip() == "/synthetic/work/", "extra account label and tooltip");
        check(extra.windowKey("session.v1") == "claude|/synthetic/work/|session.v1", "account window key");
        check(AccountIdentity{"claude", "/"}.accountLabel() == "/", "root path label");
        const AccountIdentity captured{"antigravity", "synthetic-key"};
        check(!captured.hasLocalUsage() && captured.accountLabel() == "Antigravity account"
            && captured.accountTooltip() == captured.accountLabel(), "captured key never displayed");
        AccountIdentity::antigravityLabel = [](const QString &key) -> std::optional<QString> {
            return key == "synthetic-key" ? std::optional<QString>{"Synthetic account"} : std::nullopt;
        };
        check(captured.accountLabel() == "Synthetic account" && captured.accountTooltip() == "Synthetic account", "installed account label resolver");
        const AccountIdentity composed{QString::fromUtf8("caf\xc3\xa9"), QString::fromUtf8("caf\xc3\xa9")};
        const AccountIdentity decomposed{QString::fromUtf8("cafe\xcc\x81"), QString::fromUtf8("cafe\xcc\x81")};
        check(composed == decomposed && QSet<AccountIdentity>{composed, decomposed}.size() == 1, "Swift canonical identity equality and hash");
        check(!(AccountIdentity{"claude", ""} == primary), "nil and empty account keys differ");

        auto payload = sample();
        check(payload.agents[1].accountIdentity() == AccountIdentity{"claude", "/synthetic/work"}, "snapshot identity");
        for (const auto &raw : {"", "auto", "bad", " |x", "claude| \n"})
            check(QuotaResolver::canonicalSelection(payload, raw) == "auto", "invalid selection normalizes to auto");
        for (const auto &raw : {"claude|missing", "missing|id", "claude|x|y", " claude|id"}) {
            check(QuotaResolver::canonicalSelection(std::nullopt, raw) == raw, "explicit selection retained before fetch");
            check(QuotaResolver::canonicalSelection(payload, raw) == raw && !QuotaResolver::resolve(payload, raw), "partial payload never erases explicit choice");
        }
        check(!QuotaResolver::resolve(std::nullopt, "auto"), "missing payload");
        check(QuotaResolver::canonicalSelection(payload, "claude|Shared") == "claude|session.v1", "unique raw label migration");
        auto resolved = QuotaResolver::resolve(payload, "claude|session.v1");
        check(resolved && !resolved->accountKey && resolved->window.remainingPercent == 28, "default explicit account is primary");
        resolved = QuotaResolver::resolve(payload, "claude|Shared", "/synthetic/work", {"claude"});
        check(resolved && resolved->accountKey == "/synthetic/work" && resolved->window.remainingPercent == 80, "explicit account bypasses auto exclusions");
        resolved = QuotaResolver::resolve(payload, "auto", "/synthetic/work");
        check(resolved && !resolved->accountKey && resolved->clientId == "claude", "auto ignores explicit account argument");
        check(QuotaResolver::resolve(payload, "auto", std::nullopt, {"claude"})->clientId == "codex", "auto exclusions");
        check(QuotaResolver::excludedAllCandidates(payload, "auto", {"claude", "codex"})
            && !QuotaResolver::excludedAllCandidates(payload, "claude|session.v1", {"claude", "codex"})
            && !QuotaResolver::excludedAllCandidates(payload, "auto", {})
            && !QuotaResolver::excludedAllCandidates(std::nullopt, "auto", {"claude"}), "all-hidden differs from unavailable");
        auto duplicate = payload.agents[0].windows[0];
        duplicate.cardId = "other";
        payload.agents[0].windows.append(duplicate);
        check(QuotaResolver::canonicalSelection(payload, "claude|Shared") == "claude|Shared"
            && !QuotaResolver::resolve(payload, "claude|Shared"), "ambiguous raw label cannot migrate after qualification");
        payload.agents[0].windows[1].label = "session.v1";
        check(QuotaResolver::canonicalSelection(payload, "claude|session.v1") == "claude|session.v1", "exact card ID wins over label");

        payload = sample();
        payload.agents[1].windows[0].remainingPercent = 28;
        check(!QuotaResolver::resolve(payload, "auto")->accountKey, "auto ties preserve payload order");
        payload.agents[1].windows[0].remainingPercent = 5;
        check(QuotaResolver::resolve(payload, "auto")->accountKey == "/synthetic/work", "auto considers every account");
        payload.agents[1].error = "synthetic failure";
        check(!QuotaResolver::resolve(payload, "auto")->accountKey, "auto rejects errored accounts");
        check(QuotaResolver::resolve(payload, "claude|session.v1", "/synthetic/work").has_value(), "explicit selection retains errored account reading");
        payload.agents[0].windows[0].remainingPercent = std::numeric_limits<double>::quiet_NaN();
        check(QuotaResolver::resolve(payload, "auto")->clientId == "codex", "auto rejects nonfinite readings");
        payload.agents[2].error = "synthetic failure";
        check(!QuotaResolver::excludedAllCandidates(payload, "auto", {"claude", "codex"}), "no healthy candidates is not all-hidden");
        payload = sample();
        payload.agents[0].windows[0].paceStatus.state = UsagePaceState::unavailable;
        payload.agents[0].windows[0].paceStatus.reason = UsagePaceUnavailableReason::invalidEvidence;
        check(QuotaResolver::resolve(payload, "auto")->clientId == "claude", "pace invalidity need not invalidate gauge");

        payload = sample();
        const auto now = *parseRFC3339("2026-07-10T12:00:00Z");
        auto summary = QuotaSummaryFold::build(payload, {}, PaceMode::historical, now);
        check(summary && summary->tightestClient == "claude" && !summary->tightestAccountKey
            && summary->otherWindows == 2 && summary->othersComfortable == 2 && summary->allOthersComfortable(), "summary counts distinct accounts with same card id");
        check(summary->paceCheckedWindows == 1 && summary->burning && summary->burning->aheadPercent == 40
            && summary->burning->riskText == QString::fromUtf8("\xe2\x89\x88 75% run-out risk")
            && summary->burning->etaText == "Projected empty in 1h", "burn check includes tightest and reuses presentation");
        summary = QuotaSummaryFold::build(payload, {}, PaceMode::off, now);
        check(summary && summary->paceCheckedWindows == 0 && !summary->burning, "off pace is unmeasured");
        summary = QuotaSummaryFold::build(payload, {"claude"}, PaceMode::historical, now);
        check(summary && summary->otherWindows == 0 && !summary->allOthersComfortable() && !summary->burning, "summary shares resolver exclusions");
        check(!QuotaSummaryFold::build(payload, {"claude", "codex"}, PaceMode::historical, now)
            && !QuotaSummaryFold::build(std::nullopt), "summary requires healthy window");
        auto burning = payload.agents[0].windows[0];
        burning.remainingPercent = 70;
        burning.usedPercent = 30;
        burning.historicalPace->expectedUsedPercent = 5;
        payload.agents[1].windows = {burning};
        payload.agents[0].windows[0].historicalPace->expectedUsedPercent = 60;
        summary = QuotaSummaryFold::build(payload, {}, PaceMode::historical, now);
        check(summary->burning && summary->burning->accountKey == "/synthetic/work"
            && summary->burning->aheadPercent == 25 && summary->paceCheckedWindows == 2, "fastest burn can differ from tightest account");

        QList<QuotaCycle> cycles;
        for (int i = 0; i < 18; ++i) cycles.append(QuotaCycle{100, 0, double(i + 1), 2, .5, 10, 90, i == 17 ? 99.0 : 30.0});
        const auto rows = QuotaOverviewFold::summaries({
            {"claude", std::nullopt, "empty", "Empty", {}},
            {"claude", std::nullopt, "session.v1", "Session", cycles},
            {"claude", "/synthetic/work", "session.v1", "Session", {QuotaCycle{100, 0, 120, 2, .5, 10, 90, 100}}},
            {"codex", std::nullopt, "quiet", "Quiet", {QuotaCycle{100, 0, 60, 2, .5, 10, 90, 98.9}}}});
        check(rows.size() == 3 && rows[0].peakPercent == 100 && rows[1].peakPercent == 99 && rows[2].neverExhausted, "overview omits empty and orders lifetime peaks");
        check(rows[1].cycleCount == 18 && rows[1].recent.size() == 16 && rows[1].recent.first() == 16
            && rows[1].recent.last() == 1 && rows[1].recentPeaks.first() == 30 && !rows[1].neverExhausted, "strip reverses newest sixteen but exhaustion covers all cycles");
        check(rows[0].recent.first() == 120 && rows[0].id() == "claude|/synthetic/work|session.v1"
            && rows[1].id() == "claude|session.v1", "consumption stays unclamped and ids include accounts");
        const QuotaHeatmapWindow heatmap{"claude", "session.v1", "Session", 0, "/synthetic/work"};
        check(heatmap.id() == rows[0].id(), "heatmap id agrees with summary id");
        const QuotaCycle cycle{100, 20, 4, 2, .5, 10};
        check(cycle.evidenceStartMs() == 10 && cycle.durationMs() == 80 && cycle.peakUsedPercent == 4
            && cycle.risingRuns == 1, "cycle value preserves Swift defaults");
        std::cout << "quota unit tests OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
