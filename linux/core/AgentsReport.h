#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
struct AgentReportEntry {
    QString agent;
    QList<QString> clients;
    qint64 input;
    qint64 output;
    qint64 cacheRead;
    qint64 cacheWrite;
    qint64 reasoning;
    qint64 total;
    double cost;
    qint64 messages;
    static AgentReportEntry decode(const json::Decoder &decoder);
};
struct AgentsReport {
    QList<AgentReportEntry> entries;
    double totalCost;
    qint64 totalMessages;
    static AgentsReport decode(const json::Decoder &decoder);
};
}
