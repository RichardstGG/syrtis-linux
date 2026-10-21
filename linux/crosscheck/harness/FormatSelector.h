#pragma once

#include <QtCore/QDir>
#include "Format.h"
#include "UsagePace.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace FormatSelectorDetail {
inline void require(bool condition)
{
    if (!condition) throw std::runtime_error("invalid format.json");
}

inline qint64 integer(const QJsonValue &value)
{
    const auto sentinel = std::numeric_limits<qint64>::min();
    const auto result = value.toInteger(sentinel);
    require(value.isDouble() && (result != sentinel || value.toDouble() == -0x1p63));
    return result;
}

inline qint64 argumentInteger(const QJsonValue &value)
{
    if (!value.isDouble()) return 0;
    // QJson retains Int64 JSON integers; do not round-trip through Double.
    const auto sentinel = std::numeric_limits<qint64>::min();
    const auto result = value.toInteger(sentinel);
    if (result != sentinel) return result;
    const auto number = value.toDouble();
    require(std::isfinite(number) && number >= -0x1p63 && number < 0x1p63);
    return static_cast<qint64>(number);
}

inline void validateGraph(const QJsonObject &graph)
{
    // Validate the Graph.swift projection used here before Qt's permissive
    // JSON accessors could silently turn malformed daily figures into zero.
    require(graph["contributions"].isArray());
    for (const auto &item : graph["contributions"].toArray()) {
        require(item.isObject());
        const auto entry = item.toObject();
        require(entry["date"].isString() && entry["totals"].isObject() && entry["clients"].isArray());
        const auto totals = entry["totals"].toObject();
        integer(totals["tokens"]);
        require(totals["cost"].isDouble());
        for (const auto &stripe : entry["clients"].toArray()) {
            require(stripe.isObject());
            const auto client = stripe.toObject();
            require(client["client"].isString() && client["cost"].isDouble() && client["tokens"].isObject());
            const auto lanes = client["tokens"].toObject();
            for (const auto *lane : {"input", "output", "cacheRead", "cacheWrite", "reasoning"})
                integer(lanes[QLatin1String(lane)]);
        }
    }
}
}

inline QJsonObject runFormat(const QDir &fixtures)
{
    using namespace FormatSelectorDetail;
    QFile input(fixtures.filePath(QStringLiteral("format.json")));
    if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read format.json");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    require(error.error == QJsonParseError::NoError && document.isObject());
    const auto file = document.object();
    require(file["graph"].isObject() && file["cases"].isArray());
    const auto graph = file["graph"].toObject();
    validateGraph(graph);
    QJsonObject output;
    for (const auto &item : file["cases"].toArray()) {
        require(item.isObject());
        const auto c = item.toObject();
        const auto arg = c["arg"];
        require(c["name"].isString() && c["fn"].isString());
        require(arg.isUndefined() || arg.isNull() || arg.isDouble() || arg.isString());
        require(c["now"].isUndefined() || c["now"].isNull() || c["now"].isString());
        const auto name = c["name"].toString();
        const auto fn = c["fn"].toString();
        using Syrtis::Format;
        if (fn == QStringLiteral("compactTokens")) output[name] = Format::compactTokens(argumentInteger(arg));
        else if (fn == QStringLiteral("exactTokens")) output[name] = Format::exactTokens(argumentInteger(arg));
        else if (fn == QStringLiteral("usd")) output[name] = Format::usd(arg.toDouble());
        else if (fn == QStringLiteral("monthDay")) output[name] = Format::monthDay(arg.toString());
        else if (fn == QStringLiteral("mmdd")) output[name] = Format::mmdd(arg.toString());
        else if (fn == QStringLiteral("paceDurationText")) output[name] = Format::paceDurationText(arg.toDouble());
        else if (fn == QStringLiteral("relativeTime") || fn == QStringLiteral("todayKey")
                 || fn == QStringLiteral("todayTokens") || fn == QStringLiteral("todayCost")
                 || fn == QStringLiteral("todayTopClient")) {
            const auto parsed = Syrtis::parseRFC3339(c["now"].toString());
            if (!parsed) {
                output[name] = QJsonObject{{"error", fn + QStringLiteral(" needs now")}};
                continue;
            }
            const auto now = QDateTime::fromString(c["now"].toString(), Qt::ISODateWithMs);
            if (fn == QStringLiteral("relativeTime")) {
                const auto epoch = argumentInteger(arg);
                require(epoch >= 0);
                output[name] = Format::relativeTime(quint64(epoch), now);
            } else if (fn == QStringLiteral("todayKey")) output[name] = Format::todayKey(now);
            else if (fn == QStringLiteral("todayTokens")) output[name] = Format::todayTokens(graph, now);
            else if (fn == QStringLiteral("todayCost")) output[name] = Format::todayCost(graph, now);
            else {
                const auto client = Format::todayTopClient(graph, now);
                output[name] = client ? QJsonValue(*client) : QJsonValue(QJsonValue::Null);
            }
        } else output[name] = QJsonObject{{"error", QStringLiteral("unknown fn ") + fn}};
    }
    return output;
}
