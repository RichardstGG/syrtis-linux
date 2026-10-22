#include "WindowResolution.h"

namespace syrtis::core::WindowResolver {
std::optional<qint64> firstUsageAfterReset(const QList<WindowMessage> &messages, qint64 resetMs)
{
    std::optional<qint64> first;
    for (const auto &message : messages)
        if (message.timestamp >= resetMs && (!first || message.timestamp < *first)) first = message.timestamp;
    return first;
}
WindowResolution resolve(std::optional<qint64> resetsAtMs, std::optional<qint64> durationMs,
                         qint64 now, std::optional<qint64> firstUsageAfterReset)
{
    if (!resetsAtMs || !durationMs) return {};
    const auto reset = *resetsAtMs, duration = *durationMs;
    if (reset > now) {
        if (reset - now > duration) return {};
        return {WindowResolution::Kind::active, reset - duration, reset};
    }
    if (now - reset > duration) return {};
    if (!firstUsageAfterReset) return {WindowResolution::Kind::idle};
    return {WindowResolution::Kind::inferred, *firstUsageAfterReset, *firstUsageAfterReset + duration};
}
}
