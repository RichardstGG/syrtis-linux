#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
struct HourlyReportEntry {
    QString hour;
    QList<QString> clients;
    QList<QString> models;
    qint64 input;
    qint64 output;
    qint64 cacheRead;
    qint64 cacheWrite;
    qint64 reasoning;
    qint64 total;
    qint64 messageCount;
    qint64 turnCount;
    double cost;
    static HourlyReportEntry decode(const json::Decoder &decoder);
};
struct HourlyReport {
    QList<HourlyReportEntry> entries;
    double totalCost;
    static HourlyReport decode(const json::Decoder &decoder);
};
}
