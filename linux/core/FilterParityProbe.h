#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
enum class FilterParityStatus { match, mismatch, sourceChanged, tokenUnavailable };
QString smokeLabel(FilterParityStatus status);
struct FilterParityAggregate {
    qint64 entryCount;
    qint64 input;
    qint64 output;
    qint64 cacheRead;
    qint64 cacheWrite;
    qint64 reasoning;
    qint64 totalTokens;
    qint64 messageCount;
    double totalCost;
    static FilterParityAggregate decode(const json::Decoder &decoder);
};
struct FilterParityReport {
    FilterParityStatus status;
    std::optional<FilterParityAggregate> unfiltered;
    std::optional<FilterParityAggregate> full;
    std::optional<FilterParityAggregate> delta;
    static FilterParityReport decode(const json::Decoder &decoder);
    QString smokeSummary() const;
};
struct FilterParityProbe {
    FilterParityReport hourly;
    FilterParityReport agents;
    qint64 presentClientCount;
    static FilterParityProbe decode(const json::Decoder &decoder);
    QString smokeSummary() const;
};
}
