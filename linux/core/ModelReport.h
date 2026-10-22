#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
struct ModelReportEntry {
    QString client;
    QString model;
    QString provider;
    qint64 input;
    qint64 output;
    qint64 cacheRead;
    qint64 cacheWrite;
    qint64 reasoning;
    qint64 total;
    qint64 messageCount;
    double cost;
    std::optional<double> msPer1kTokens;
    std::optional<double> costEstimate;
    static ModelReportEntry decode(const json::Decoder &decoder);
};
struct ModelReport {
    QList<ModelReportEntry> entries;
    qint64 totalInput;
    qint64 totalOutput;
    qint64 totalCacheRead;
    qint64 totalCacheWrite;
    qint64 totalMessages;
    double totalCost;
    std::optional<quint64> pricingUpdatedAt;
    static ModelReport decode(const json::Decoder &decoder);
};
}
