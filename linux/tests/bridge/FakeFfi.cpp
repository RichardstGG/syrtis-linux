#include "FakeFfi.h"

extern "C" {
#include "ctb.h"
}

#include <QCoreApplication>
#include <QSet>
#include <QThread>

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <utility>

namespace FakeFfi {
namespace {
std::mutex mutex;
Stats state;
QSet<char *> allocations;
std::optional<QByteArray> reply;
QByteArray blockedOperation;

bool onMainThread()
{
    return QThread::currentThread() == QCoreApplication::instance()->thread();
}
} // namespace

QSemaphore entered;
QSemaphore released;

void reset()
{
    const std::lock_guard lock(mutex);
    Q_ASSERT(allocations.isEmpty());
    state = {};
    reply.reset();
    blockedOperation.clear();
    entered.tryAcquire(entered.available());
    released.tryAcquire(released.available());
}

void setReply(QByteArray json)
{
    const std::lock_guard lock(mutex);
    reply = std::move(json);
}

void block(QByteArray operation)
{
    const std::lock_guard lock(mutex);
    blockedOperation = std::move(operation);
}

Stats stats()
{
    const std::lock_guard lock(mutex);
    return state;
}

static char *respond(const char *operation, QList<QByteArray> arguments = {})
{
    QByteArray json;
    bool blocked;
    {
        const std::lock_guard lock(mutex);
        state.calls.append({operation, std::move(arguments), onMainThread()});
        blocked = blockedOperation == operation;
        json = reply.value_or(QByteArray(operation) == "tb_probe"
            ? QByteArray(R"({"ok":true,"messages":1})")
            : QByteArray(R"({"ok":true,"data":{"synthetic":true,"totalTokens":150},"extra":"retained"})"));
    }
    if (blocked) {
        entered.release();
        const bool timedOut = !released.tryAcquire(1, 5000);
        const std::lock_guard lock(mutex);
        state.gateTimedOut |= timedOut;
    }
    if (json.isNull())
        return nullptr;
    auto *p = static_cast<char *>(std::malloc(json.size() + 1));
    if (!p)
        std::abort();
    std::memcpy(p, json.constData(), json.size() + 1);
    const std::lock_guard lock(mutex);
    allocations.insert(p);
    ++state.allocated;
    return p;
}

} // namespace FakeFfi

// Defined against the real header: signature drift is a compile error.
extern "C" {
char *tb_probe() { return FakeFfi::respond("tb_probe"); }
char *tb_graph(const char *year) { return FakeFfi::respond("tb_graph", {year}); }
char *tb_refresh_graph(const char *year) { return FakeFfi::respond("tb_refresh_graph", {year}); }
char *tb_model_report(const char *year) { return FakeFfi::respond("tb_model_report", {year}); }
char *tb_hourly_report(const char *year, const char *clients)
{
    return FakeFfi::respond("tb_hourly_report", {year, clients});
}
char *tb_agents_report(const char *year, const char *clients)
{
    return FakeFfi::respond("tb_agents_report", {year, clients});
}
char *tb_filter_parity_probe() { return FakeFfi::respond("tb_filter_parity_probe"); }
char *tb_usage_trace(std::int64_t secs)
{
    return FakeFfi::respond("tb_usage_trace", {QByteArray::number(static_cast<qlonglong>(secs))});
}
char *tb_tokens_per_min() { return FakeFfi::respond("tb_tokens_per_min"); }
char *tb_quota_provider_ids() { return FakeFfi::respond("tb_quota_provider_ids"); }
char *tb_client_ids() { return FakeFfi::respond("tb_client_ids"); }
char *tb_agent_usage() { return FakeFfi::respond("tb_agent_usage"); }
char *tb_quota_curve(const char *client, const char *account, const char *window, std::uint64_t gen)
{
    return FakeFfi::respond("tb_quota_curve",
                          {client, account, window, QByteArray::number(static_cast<qulonglong>(gen))});
}
char *tb_window_usage(const char *account, std::int64_t from, std::int64_t until)
{
    return FakeFfi::respond("tb_window_usage", {account,
        QByteArray::number(static_cast<qlonglong>(from)),
        QByteArray::number(static_cast<qlonglong>(until))});
}
char *tb_set_extra_scan_paths(const char *json)
{
    return FakeFfi::respond("tb_set_extra_scan_paths", {json});
}
void tb_free(char *p)
{
    if (!p)
        return;
    const std::lock_guard lock(FakeFfi::mutex);
    if (!FakeFfi::allocations.remove(p)) {
        ++FakeFfi::state.invalidFrees;
        return;
    }
    ++FakeFfi::state.freed;
    FakeFfi::state.freedOnMainThread |= FakeFfi::onMainThread();
    std::memset(p, 0xDD, std::strlen(p));
    std::free(p);
}
}
