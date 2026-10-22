#pragma once
#include "JsonDecoder.h"
#include <QtCore/QSet>

namespace syrtis::core {
struct TraceBucket {
    QString client;
    QString agent;
    QString model;
    qint64 tokens;
    qint64 messages;
    double tokensPerMin;
    static TraceBucket decode(const json::Decoder &decoder);
    static double totalRate(const QList<TraceBucket> &buckets, const QSet<QString> &hidden);
    static QList<TraceBucket> collapseByClient(const QList<TraceBucket> &buckets);
};
struct TokensPerMin {
    double tokensPerMin;
    static TokensPerMin decode(const json::Decoder &decoder);
};
}
