#pragma once
#include <QtCore/QList>
#include <optional>

namespace syrtis::core {
// Timestamp projection of a WindowUsage.swift row; the resolver reads no other fields.
struct WindowMessage {
    qint64 timestamp = 0;
};
struct WindowResolution {
    enum class Kind { active, idle, inferred, unavailable };
    Kind kind = Kind::unavailable;
    qint64 start = 0;
    qint64 end = 0;
    bool operator==(const WindowResolution &) const = default;
};
namespace WindowResolver {
std::optional<qint64> firstUsageAfterReset(const QList<WindowMessage> &messages, qint64 resetMs);
WindowResolution resolve(std::optional<qint64> resetsAtMs, std::optional<qint64> durationMs,
                         qint64 now, std::optional<qint64> firstUsageAfterReset);
}
}
