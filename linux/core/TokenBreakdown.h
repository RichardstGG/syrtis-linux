#pragma once
#include "JsonDecoder.h"

// Single definition shared by the Graph and attribution ports (Swift defines it once).
namespace syrtis::core {
qint64 saturatingAdding(qint64 a, qint64 b);
struct TokenBreakdown {
    qint64 input = 0, output = 0, cacheRead = 0, cacheWrite = 0, reasoning = 0;
    qint64 total() const;
    bool operator==(const TokenBreakdown &) const = default;
    static TokenBreakdown decode(const json::Decoder &decoder);
};
}
