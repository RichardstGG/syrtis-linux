#pragma once
#include "WindowUsage.h"

namespace syrtis::core {
enum class QuotaMetric { used, remaining };
inline double metricValue(QuotaMetric metric, double used) { return metric == QuotaMetric::used ? used : 100 - used; }
struct QuotaSample {
    qint64 atMs;
    double usedPercent;
    bool operator==(const QuotaSample &) const = default;
};
struct BarRect {
    double x, width, height;
    bool isEmpty;
    bool operator==(const BarRect &) const = default;
};
struct CurvePoint {
    double x, y;
    bool operator==(const CurvePoint &) const = default;
};
struct HitZone {
    qsizetype index;
    qint64 loMs, hiMs;
    double x, width;
    std::optional<QuotaSample> closingSample, openingSample;
    std::optional<double> consumed(QuotaMetric metric) const;
    bool operator==(const HitZone &) const = default;
};
struct ChartGeometry {
    double nowX, firstSampleX;
    QList<BarRect> bars;
    QList<HitZone> hits;
    QList<CurvePoint> samplePoints, curve;
    bool operator==(const ChartGeometry &) const = default;
};
namespace WindowCardGeometry {
inline constexpr int curveResolution = 8;
struct UsageGeometry { QList<BarRect> bars; QList<HitZone> hits; };
struct QuotaGeometry { QList<CurvePoint> samplePoints, curve; double nowX, firstSampleX; };
QList<HitZone> zones(qint64 windowStartMs, qint64 windowEndMs, qint64 nowMs, const QList<QuotaSample> &samples);
UsageGeometry usageGeometry(qint64 windowStartMs, qint64 windowEndMs, qint64 nowMs,
                            const QList<QuotaSample> &samples, const QList<WindowMessage> &messages);
QuotaGeometry quotaGeometry(qint64 windowStartMs, qint64 windowEndMs, qint64 nowMs,
                            const QList<QuotaSample> &samples, QuotaMetric metric);
ChartGeometry chartGeometry(qint64 windowStartMs, qint64 windowEndMs, qint64 nowMs,
                            const QList<QuotaSample> &samples, const QList<WindowMessage> &messages, QuotaMetric metric);
QList<CurvePoint> monotoneCurve(const QList<CurvePoint> &points);
}
}
