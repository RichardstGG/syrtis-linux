#pragma once

#include "Bridge.h"
#include "QuotaSelectionPolicy.h"
#include <QTimer>
#include <QVariant>
#include <functional>

namespace Syrtis {
// Quota polling/retention from DashboardModel.swift and AgentUsageThrottle in
// UsageDataSource.swift. Own on the GUI thread: Bridge dispatches blocking FFI
// on its dedicated quota worker. No provider IO or aggregation lives here.
class QuotaService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariant remainingPercent READ remainingPercent NOTIFY stateChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool attempted READ attempted NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    using Clock = std::function<QDateTime()>;
    explicit QuotaService(QObject *parent = nullptr, Clock now = QDateTime::currentDateTimeUtc);

    const std::optional<AgentUsagePayload> &payload() const { return payload_; }
    const std::optional<ResolvedQuota> &reading() const { return reading_; }
    std::optional<QDateTime> resolvedAt() const { return resolvedAt_; }
    QVariant remainingPercent() const;
    bool stale() const;
    bool busy() const { return requestId_ != 0; }
    bool attempted() const { return attempted_; }
    QString error() const { return error_; }

    // Auto considers every account, exactly as QuotaResolver does. Explicit
    // selection uses accountKey (nullopt = primary). Context changes discard
    // last-good and in-flight publications, including switch-away/switch-back.
    void setSelection(QString selection, std::optional<QString> accountKey = std::nullopt,
                      QSet<QString> excluding = {});
    // Call when credential bindings/registry change, even for primary (nil key).
    void invalidateAccounts();

public slots:
    void refresh();
    void start(int intervalMs = 60000);
    void stop();

signals:
    void stateChanged();

private:
    void complete(quint64 requestId, const QJsonObject &envelope);
    void selectReading();
    void schedule();

    Bridge bridge_;
    QTimer timer_;
    Clock now_;
    QString selection_ = QuotaResolver::automatic;
    std::optional<QString> accountKey_;
    QSet<QString> excluding_;
    std::optional<AgentUsagePayload> payload_;
    std::optional<ResolvedQuota> reading_;
    std::optional<QDateTime> resolvedAt_, fetchedAt_;
    std::optional<quint64> highestGeneration_;
    quint64 requestId_ = 0;
    int intervalMs_ = 60000;
    bool polling_ = false, attempted_ = false, failed_ = false, providerFailed_ = false;
    QString error_;
};
} // namespace Syrtis
