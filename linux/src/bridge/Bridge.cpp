#include "Bridge.h"

extern "C" {
#include "ctb.h"
}

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrentRun>

#include <memory>
#include <utility>

namespace Syrtis {
namespace {

QByteArray owned(const QByteArray &value)
{
    // Also copy fromRawData buffers, whose storage QByteArray does not own.
    return value.isNull() ? QByteArray() : QByteArray(value.constData(), value.size());
}

const char *cString(const QByteArray &value)
{
    return value.isNull() ? nullptr : value.constData();
}

QJsonObject failure(const char *code)
{
    return {{"ok", false}, {"err", QLatin1String(code)}};
}

QJsonObject invoke(const std::function<char *()> &call, bool probe)
{
    const std::unique_ptr<char, decltype(&tb_free)> json(call(), &tb_free);
    if (!json)
        return failure("bridge_null_response");

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray(json.get()), &error);
    if (error.error != QJsonParseError::NoError)
        return failure("bridge_invalid_json");

    const auto envelope = document.object();
    const auto ok = envelope.value("ok");
    const bool successData = probe ? envelope.value("messages").isDouble()
                                   : envelope.contains("data");
    if (!document.isObject() || !ok.isBool()
        || (ok.toBool() ? !successData : !envelope.value("err").isString()))
        return failure("bridge_invalid_envelope");
    return envelope;
}

// Process-lifetime pools: destroying a Bridge never joins a blocking FFI call.
// A slow quota provider cannot occupy the reports worker. Qt drains these pools
// at process teardown; individual FFI calls cannot be cancelled (ADR-003).
struct Pools {
    QThreadPool reports;
    QThreadPool quota;
    Pools()
    {
        reports.setMaxThreadCount(1);
        quota.setMaxThreadCount(1);
    }
};

} // namespace

quint64 Bridge::submit(std::function<char *()> call, Lane lane, bool probe)
{
    Q_ASSERT(QThread::currentThread() == thread());
    static Pools pools;
    const auto id = ++nextId_;
    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, id] {
        const auto envelope = watcher->result(); // Already finished; never waits.
        watcher->deleteLater();
        emit completed(id, envelope);
    });
    watcher->setFuture(QtConcurrent::run(lane == Lane::Quota ? &pools.quota : &pools.reports,
                                       [call = std::move(call), probe] { return invoke(call, probe); }));
    return id;
}

quint64 Bridge::probe() { return submit(tb_probe, Lane::Report, true); }
quint64 Bridge::graph(QByteArray year)
{
    return submit([year = owned(year)] { return tb_graph(cString(year)); });
}
quint64 Bridge::refreshGraph(QByteArray year)
{
    return submit([year = owned(year)] { return tb_refresh_graph(cString(year)); });
}
quint64 Bridge::modelReport(QByteArray year)
{
    return submit([year = owned(year)] { return tb_model_report(cString(year)); });
}
quint64 Bridge::hourlyReport(QByteArray year, QByteArray clients)
{
    return submit([year = owned(year), clients = owned(clients)] {
        return tb_hourly_report(cString(year), cString(clients));
    });
}
quint64 Bridge::agentsReport(QByteArray year, QByteArray clients)
{
    return submit([year = owned(year), clients = owned(clients)] {
        return tb_agents_report(cString(year), cString(clients));
    });
}
quint64 Bridge::filterParityProbe() { return submit(tb_filter_parity_probe); }
quint64 Bridge::usageTrace(std::int64_t windowSecs)
{
    return submit([windowSecs] { return tb_usage_trace(windowSecs); });
}
quint64 Bridge::tokensPerMin() { return submit(tb_tokens_per_min); }
quint64 Bridge::quotaProviderIds() { return submit(tb_quota_provider_ids); }
quint64 Bridge::clientIds() { return submit(tb_client_ids); }
quint64 Bridge::agentUsage() { return submit(tb_agent_usage, Lane::Quota); }
quint64 Bridge::quotaCurve(QByteArray clientId, QByteArray accountKey,
                          QByteArray windowKey, std::uint64_t generation)
{
    return submit([clientId = owned(clientId), accountKey = owned(accountKey),
                   windowKey = owned(windowKey), generation] {
        return tb_quota_curve(cString(clientId), cString(accountKey), cString(windowKey), generation);
    }, Lane::Quota);
}
quint64 Bridge::windowUsage(QByteArray accountKey, std::int64_t fromMs, std::int64_t untilMs)
{
    return submit([accountKey = owned(accountKey), fromMs, untilMs] {
        return tb_window_usage(cString(accountKey), fromMs, untilMs);
    });
}
quint64 Bridge::setExtraScanPaths(QByteArray json)
{
    return submit([json = owned(json)] { return tb_set_extra_scan_paths(cString(json)); });
}

} // namespace Syrtis
