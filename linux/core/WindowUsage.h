#pragma once
#include "TokenBreakdown.h"

namespace syrtis::core {
// WindowUsage.swift value dependency of the history and geometry ports.
struct WindowMessage {
    qint64 timestamp = 0;
    QString client, providerId, modelId;
    qint64 input = 0, output = 0, cacheRead = 0, cacheWrite = 0, reasoning = 0;
    double cost = 0;
    bool isTurnStart = false;
    qint64 tokens() const;
    qint64 tokensExCacheRead() const;
    static WindowMessage decode(const json::Decoder &d);
    bool operator==(const WindowMessage &) const = default;
};
}
