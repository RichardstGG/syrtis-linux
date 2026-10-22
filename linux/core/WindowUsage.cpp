#include "WindowUsage.h"

namespace syrtis::core {
namespace {
// WindowMessage deliberately clamps either overflow direction to Int64.max,
// unlike the signed saturation used by TokenBreakdown and the folds.
qint64 messageAdd(qint64 a, qint64 b) {
    if ((b > 0 && a > std::numeric_limits<qint64>::max() - b)
        || (b < 0 && a < std::numeric_limits<qint64>::min() - b))
        return std::numeric_limits<qint64>::max();
    return a + b;
}
}
qint64 WindowMessage::tokens() const {
    return messageAdd(messageAdd(messageAdd(messageAdd(input, output), cacheRead), cacheWrite), reasoning);
}
qint64 WindowMessage::tokensExCacheRead() const {
    return messageAdd(messageAdd(messageAdd(input, output), cacheWrite), reasoning);
}
WindowMessage WindowMessage::decode(const json::Decoder &d) {
    return {d.field<qint64>("timestamp"), d.field<QString>("client"),
        d.field<QString>("providerId"), d.field<QString>("modelId"),
        d.field<qint64>("input"), d.field<qint64>("output"), d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"), d.field<qint64>("reasoning"),
        d.field<double>("cost"), d.field<bool>("isTurnStart")};
}
}
