#include "QuotaTrendSelector.h"
#include "../../core/QuotaHeatmap.h"
#include "../../core/QuotaTrend.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

QJsonArray array(const QJsonObject &object, const char *key)
{
    require(object[key].isArray(), "expected fixture array");
    return object[key].toArray();
}

QJsonObject object(const QJsonValue &value)
{
    require(value.isObject(), "expected fixture object");
    return value.toObject();
}

double number(const QJsonObject &object, const char *key)
{
    require(object[key].isDouble() && std::isfinite(object[key].toDouble()), "expected finite number");
    return object[key].toDouble();
}

qint64 integer(const QJsonObject &object, const char *key)
{
    number(object, key);
    const auto value = object[key].toInteger();
    require(QJsonValue(value) == object[key], "expected Int64");
    return value;
}

QString string(const QJsonObject &object, const char *key)
{
    require(object[key].isString(), "expected fixture string");
    return object[key].toString();
}
}

QJsonObject quotaTrendSelector(const QDir &fixtures)
{
    QFile file(fixtures.filePath(QStringLiteral("quota-trend.json")));
    require(file.open(QIODevice::ReadOnly), "cannot open quota-trend.json");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), "invalid quota-trend.json");
    const auto fixture = document.object();
    QJsonObject output;
    auto insert = [&output](const QJsonObject &row, const QJsonValue &value) {
        const auto id = string(row, "id");
        require(!id.isEmpty() && !output.contains(id), "empty or duplicate fixture id");
        output.insert(id, value);
    };

    for (const auto &value : array(fixture, "heatmap")) {
        const auto row = object(value);
        const QTimeZone zone(string(row, "timeZone").toUtf8());
        require(zone.isValid(), "invalid heatmap time zone");
        std::vector<syrtis::QuotaCurvePoint> points;
        for (const auto &pointValue : array(row, "points")) {
            const auto point = object(pointValue);
            const auto sampledAt = integer(point, "sampledAt");
            const auto used = number(point, "usedPercent");
            const auto resetAt = integer(point, "resetAt");
            const auto duration = integer(point, "durationSeconds");
            const auto source = string(point, "durationSource");
            const auto origin = string(point, "origin");
            // Match the shipping Swift QuotaCurvePoint decoder's admission.
            require(duration >= 1 && duration <= 400 * 86400 && used >= 0 && used <= 100
                        && resetAt >= std::numeric_limits<qint64>::min() + duration
                        && sampledAt >= resetAt - duration && sampledAt <= resetAt
                        && (source == "provider" || source == "contract" || source == "observed")
                        && (origin == "liveV3" || origin == "importedV2")
                        && point["isActiveGroup"].isBool(), "invalid quota curve point");
            points.push_back({sampledAt, used, resetAt});
        }
        const auto heatmap = syrtis::QuotaHeatmapFold::build(points, zone);
        QJsonArray cells;
        for (const auto &hours : heatmap.cells) {
            QJsonArray day;
            for (double cell : hours)
                day.append(cell);
            cells.append(day);
        }
        insert(row, QJsonObject{{"cells", cells}, {"peak", heatmap.peak}, {"total", heatmap.total},
                               {"unplacedPercent", heatmap.unplacedPercent}, {"observedDays", heatmap.observedDays},
                               {"isEmpty", heatmap.isEmpty()}, {"hasMovement", heatmap.hasMovement()}});
    }

    for (const auto &value : array(fixture, "trend")) {
        const auto row = object(value);
        std::vector<syrtis::QuotaSample> samples;
        for (const auto &sampleValue : array(row, "samples")) {
            const auto sample = object(sampleValue);
            samples.push_back({integer(sample, "atMs"), number(sample, "usedPercent")});
        }
        const auto trend = syrtis::QuotaTrendFold::trend(
            number(row, "usedPercent"), integer(row, "windowStartMs"), integer(row, "windowEndMs"),
            integer(row, "nowMs"), samples);
        if (!trend) {
            insert(row, QJsonValue::Null);
            continue;
        }
        const char *direction = trend->direction == syrtis::QuotaTrend::Direction::flat ? "flat"
            : (trend->direction == syrtis::QuotaTrend::Direction::rising ? "rising" : "falling");
        insert(row, QJsonObject{{"direction", direction}, {"projectedUsedPercent", trend->projectedUsedPercent},
                               {"projectedDeltaPercent", trend->projectedDeltaPercent},
                               {"runsOutEarly", trend->runsOutEarly()}});
    }
    return output;
}
