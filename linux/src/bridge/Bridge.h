#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

#include <cstdint>
#include <functional>

namespace Syrtis {

// Asynchronous Linux C ABI surface (ctb.h). macOS-only/v1-excluded entry
// points are deliberately not exposed; no raw FFI pointer crosses this class.
// Call on the GUI thread and consume completed there. Inputs are copied before
// dispatch; a null QByteArray maps to NULL, an empty non-null one to "".
// Destruction drops notifications without waiting for uncancellable FFI work.
class Bridge final : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    quint64 probe();
    quint64 graph(QByteArray year = {});
    quint64 refreshGraph(QByteArray year = {});
    quint64 modelReport(QByteArray year = {});
    quint64 hourlyReport(QByteArray year = {}, QByteArray clients = {});
    quint64 agentsReport(QByteArray year = {}, QByteArray clients = {});
    quint64 filterParityProbe();
    quint64 usageTrace(std::int64_t windowSecs);
    quint64 tokensPerMin();
    quint64 quotaProviderIds();
    quint64 clientIds();
    quint64 agentUsage();
    quint64 quotaCurve(QByteArray clientId, QByteArray accountKey,
                       QByteArray windowKey, std::uint64_t generation);
    quint64 windowUsage(QByteArray accountKey, std::int64_t fromMs, std::int64_t untilMs);
    quint64 setExtraScanPaths(QByteArray json);

signals:
    // The entire envelope is retained, including unknown/additive fields and
    // tb_probe's legacy messages field. Local failures use fixed bridge_* err
    // codes; backend errors are preserved verbatim and never logged here.
    void completed(quint64 requestId, QJsonObject envelope);

private:
    enum class Lane { Report, Quota };
    quint64 submit(std::function<char *()> call, Lane lane = Lane::Report, bool probe = false);
    quint64 nextId_ = 0;
};

} // namespace Syrtis
