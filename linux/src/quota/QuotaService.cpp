#include "QuotaService.h"
#include <QThread>
#include <utility>

namespace Syrtis {
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
            auto candidate = AgentUsagePayload::decode({envelope.value("data"), {}});
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
    if (!reading_ && canonical == QuotaResolver::automatic) return;
    const AccountIdentity selected = reading_ ? AccountIdentity{reading_->clientId, reading_->accountKey}
        : AccountIdentity{canonical.section('|', 0, 0), accountKey_};
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
