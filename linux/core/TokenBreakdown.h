#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
inline qint64 saturatingAdding(qint64 a, qint64 b)
{
    constexpr auto hi = std::numeric_limits<qint64>::max();
    constexpr auto lo = std::numeric_limits<qint64>::min();
    if (b > 0 && a > hi - b) return hi;
    if (b < 0 && a < lo - b) return lo;
    return a + b;
}
struct TokenBreakdown {
    qint64 input = 0, output = 0, cacheRead = 0, cacheWrite = 0, reasoning = 0;
    qint64 total() const {
        return saturatingAdding(saturatingAdding(saturatingAdding(
            saturatingAdding(input, output), cacheRead), cacheWrite), reasoning);
    }
    static TokenBreakdown decode(const json::Decoder &d) {
        return {d.field<qint64>("input"), d.field<qint64>("output"), d.field<qint64>("cacheRead"),
                d.field<qint64>("cacheWrite"), d.field<qint64>("reasoning")};
    }
    bool operator==(const TokenBreakdown &) const = default;
};
}
