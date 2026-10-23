#include "QuotaService.h"
#include "ClientRegistry.h"
#include "Format.h"
#include "Localization.h"
#include "QuotaSummary.h"
#include <QThread>
#include <cmath>
#include <utility>

namespace Syrtis {
namespace ClientRegistry = syrtis::core::ClientRegistry;
namespace {
const QStringList cardClients{"claude", "codex", "antigravity"};

QString remainingText(double percent)
{
    // Same integer rounding and localized format as AgentLimitsCard.swift.
    return syrtis::core::localized("%lld%% left", {syrtis::core::Format::exactTokens(qint64(std::round(percent)))});
}

bool snapshotStale(const syrtis::core::AgentUsageSnapshot &agent, const QDateTime &now)
{
    const auto updated = QDateTime::fromString(agent.updatedAt, Qt::ISODateWithMs);
    return !updated.isValid() || updated.msecsTo(now) > 30 * 60 * 1000;
}

QString loginText(const syrtis::core::AccountIdentity &identity)
{
    if (identity.clientId == "claude" && !identity.isPrimary())
        return QStringLiteral("Run `claude` to sign in with this account's CLAUDE_CONFIG_DIR, then refresh.");
    if (identity.clientId == "claude")
        return QStringLiteral("Run `claude` to sign in, or run `claude setup-token` and save the token with "
            "`secret-tool store --label='Syrtis Claude setup-token' application syrtis service "
            "tokenbar-claude-oauth-token` (requires libsecret-tools; enter the token at the prompt).");
    return QStringLiteral("Sign in to %1, then refresh to see its limits.")
        .arg(ClientRegistry::style(identity.clientId).displayName);
}
} // namespace

QuotaService::QuotaService(QObject *parent, Clock now)
    : QObject(parent), now_(std::move(now))
{
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &QuotaService::refresh);
    connect(&bridge_, &Bridge::completed, this, &QuotaService::complete);
}

QVariant QuotaService::remainingPercent() const
{
    return reading_ ? QVariant(reading_->window.remainingPercent) : QVariant();
}

bool QuotaService::stale() const
{
    return reading_ && (failed_ || providerFailed_
        || (resolvedAt_ && resolvedAt_->msecsTo(now_()) > 30 * 60 * 1000));
}

QVariantList QuotaService::cards() const
{
    QVariantList result;
    const auto now = now_();
    const auto referenceTime = now.toMSecsSinceEpoch() / 1000.0 - 978307200.0;
    for (const auto &client : cardClients) {
        QList<const syrtis::core::AgentUsageSnapshot *> snapshots;
        if (payload_) {
            for (const auto &agent : payload_->agents) {
                if (agent.clientId != client) continue;
                // An explicit account view must never fall back to another
                // account's provider result, including the primary account.
                if (selection_ != syrtis::core::QuotaResolver::automatic
                    && (client != selection_.section('|', 0, 0) || agent.accountKey != accountKey_)) continue;
                snapshots.append(&agent);
            }
        }
        if (selection_ != syrtis::core::QuotaResolver::automatic && client != selection_.section('|', 0, 0)) continue;
        if (snapshots.isEmpty()) snapshots.append(nullptr);
        for (const auto *agent : snapshots) {
            const syrtis::core::AccountIdentity identity{client, agent ? agent->accountKey : accountKey_};
            QVariantMap card{{"clientId", client}, {"id", identity.windowKey("limits")},
                {"name", ClientRegistry::style(client).displayName},
                {"accountLabel", identity.accountLabel().value_or(QString())}};
            QString state, message;
            QVariantList windows;
            if (!agent) {
                state = failed_ ? "error" : attempted_ ? "unsupported" : "loading";
                message = failed_ ? error_ : attempted_ ? "No supported quota windows reported."
                                                       : "Checking agent limits…";
            } else if (agent->source == "unconfigured" || agent->source == "needs-login") {
                state = "needs-login";
                message = loginText(identity);
            } else if (agent->source == "unsupported") {
                state = "unsupported";
                message = "Quota is not supported by this provider.";
            } else {
                for (const auto &window : agent->uniqueCardWindows()) {
                    const auto reset = window.resetsAt
                        ? syrtis::core::UsagePace::resetText(*window.resetsAt, referenceTime) : std::nullopt;
                    windows.append(QVariantMap{{"id", identity.windowKey(window.cardId)},
                        {"label", syrtis::core::localized(window.label)}, {"remainingPercent", window.remainingPercent},
                        {"remainingText", remainingText(window.remainingPercent)},
                        {"resetText", reset.value_or(window.resetText.value_or(QString()))}});
                }
                if (windows.isEmpty()) {
                    state = failed_ || agent->error ? "error" : "unsupported";
                    message = failed_ ? error_ : agent->error.value_or("No supported quota windows reported.");
                } else {
                    state = failed_ || agent->error || snapshotStale(*agent, now) ? "stale" : "fresh";
                    if (state == "stale") {
                        message = "Last known quota; refresh pending.";
                        const auto detail = failed_ ? error_ : agent->error.value_or(QString());
                        if (!detail.isEmpty()) message += " " + detail;
                    }
                }
            }
            card.insert("state", state);
            card.insert("message", message);
            card.insert("windows", windows);
            result.append(card);
        }
    }
    return result;
}

QVariantMap QuotaService::summary() const
{
    if (!payload_ || failed_) return {};
    // The summary speaks only for fresh supported cards. It must not claim
    // that a retained last-good window is a current reading.
    QList<syrtis::core::AgentUsageSnapshot> fresh;
    for (const auto &agent : payload_->agents) {
        if (!cardClients.contains(agent.clientId) || agent.isSetupPlaceholder()
            || agent.source == "needs-login" || agent.source == "unsupported"
            || agent.error || snapshotStale(agent, now_())) continue;
        if (selection_ != syrtis::core::QuotaResolver::automatic
            && (agent.clientId != selection_.section('|', 0, 0) || agent.accountKey != accountKey_)) continue;
        fresh.append(agent);
    }
    const auto summary = syrtis::core::QuotaSummaryFold::build(payload_->replacingAgents(fresh), {}, syrtis::core::PaceMode::historical,
        now_().toMSecsSinceEpoch() / 1000.0 - 978307200.0);
    if (!summary) return {};
    const syrtis::core::AccountIdentity identity{summary->tightestClient, summary->tightestAccountKey};
    const auto account = identity.accountLabel();
    const auto reset = summary->resetsAt
        ? syrtis::core::UsagePace::resetText(*summary->resetsAt, now_().toMSecsSinceEpoch() / 1000.0 - 978307200.0)
        : std::nullopt;
    return {{"name", ClientRegistry::style(summary->tightestClient).displayName
                + (account ? " " + *account : QString())},
        {"label", syrtis::core::localized(summary->tightestLabel)},
        {"remainingText", remainingText(summary->remainingPercent)},
        {"resetText", reset.value_or(QString())}};
}

void QuotaService::setSelection(QString selection, std::optional<QString> accountKey,
                               QSet<QString> excluding)
{
    if (selection_ == selection && accountKey_ == accountKey && excluding_ == excluding) return;
    selection_ = std::move(selection);
    accountKey_ = std::move(accountKey);
    excluding_ = std::move(excluding);
    invalidateAccounts();
}

void QuotaService::invalidateAccounts()
{
    Q_ASSERT(QThread::currentThread() == thread());
    requestId_ = 0; // Bridge cannot cancel FFI; its old completion is ignored.
    payload_.reset();
    reading_.reset();
    resolvedAt_.reset();
    fetchedAt_.reset();
    attempted_ = failed_ = providerFailed_ = false;
    error_.clear();
    emit stateChanged();
    if (polling_) refresh();
}

void QuotaService::refresh()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (busy()) return; // Coalesce concurrent refreshes, including timer ticks.
    if (fetchedAt_ && fetchedAt_->msecsTo(now_()) < 50000) {
        emit stateChanged(); // Age may have crossed the stale boundary.
        schedule();
        return;
    }
    requestId_ = bridge_.agentUsage();
    emit stateChanged();
}

void QuotaService::complete(quint64 requestId, const QJsonObject &envelope)
{
    if (requestId != requestId_) return;
    requestId_ = 0;
    attempted_ = true;
    std::optional<QString> failure;
    if (!envelope.value("ok").toBool()) {
        failure = envelope.value("err").toString();
    } else {
        try {
            auto candidate = syrtis::core::AgentUsagePayload::decode({envelope.value("data"), {}});
            // An older publication cannot overwrite a newer accepted value.
            if (!candidate.publicationGeneration || !highestGeneration_
                || *candidate.publicationGeneration >= *highestGeneration_) {
                if (candidate.publicationGeneration) highestGeneration_ = candidate.publicationGeneration;
                payload_ = std::move(candidate);
                fetchedAt_ = now_(); // Stamp completion, not request time.
                failed_ = false;
                selectReading();
            }
        } catch (const syrtis::core::DecodingError &) {
            failure = QStringLiteral("quota_invalid_payload");
        }
    }
    if (failure) {
        // Only an outer failure retains last-good. Neither the value nor its
        // original snapshot timestamp is refreshed by a failed attempt.
        failed_ = true;
        error_ = failure->isEmpty() ? QStringLiteral("quota_refresh_failed") : *failure;
    }
    emit stateChanged();
    schedule();
}

void QuotaService::selectReading()
{
    reading_ = QuotaSelectionPolicy::resolve(payload_, selection_, excluding_, accountKey_);
    resolvedAt_ = QuotaSelectionPolicy::resolvedAt(*payload_, selection_, excluding_, accountKey_);
    providerFailed_ = false;
    error_.clear();
    // A successful unresolved/terminal payload clears last-good, but an
    // explicit account's provider error still belongs in its empty state.
    const auto canonical = QuotaSelectionPolicy::effectiveSelection(payload_, selection_, excluding_, accountKey_);
    if (!reading_ && canonical == syrtis::core::QuotaResolver::automatic) return;
    const syrtis::core::AccountIdentity selected = reading_ ? syrtis::core::AccountIdentity{reading_->clientId, reading_->accountKey}
        : syrtis::core::AccountIdentity{canonical.section('|', 0, 0), accountKey_};
    for (const auto &agent : payload_->agents) {
        if (agent.accountIdentity() != selected) continue;
        providerFailed_ = agent.error.has_value();
        error_ = agent.error.value_or(QString());
        break;
    }
}

void QuotaService::start(int intervalMs)
{
    intervalMs_ = qMax(1, intervalMs);
    polling_ = true;
    refresh();
}

void QuotaService::stop()
{
    polling_ = false;
    timer_.stop();
    requestId_ = 0;
    emit stateChanged();
}

void QuotaService::schedule()
{
    if (polling_) timer_.start(intervalMs_);
}
} // namespace Syrtis
