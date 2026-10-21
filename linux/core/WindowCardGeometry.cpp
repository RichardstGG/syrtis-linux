#include "WindowCardGeometry.h"
#include <algorithm>

namespace syrtis::core {
std::optional<double> HitZone::consumed(QuotaMetric metric) const {
    if (!openingSample || !closingSample) return std::nullopt;
    return metricValue(metric, closingSample->usedPercent) - metricValue(metric, openingSample->usedPercent);
}
namespace WindowCardGeometry {
QList<HitZone> zones(qint64 start, qint64 end, qint64 now, const QList<QuotaSample> &samples) {
    if (now <= start || end <= start) return {};
    QList<qint64> edges{start};
    for (const auto &s : samples)
        if (s.atMs > start && s.atMs < now && s.atMs != edges.last()) edges.append(s.atMs);
    edges.append(now);
    const double span = double(end - start);
    auto sampleAt = [&](qint64 stamp) -> std::optional<QuotaSample> {
        for (const auto &s : samples) if (s.atMs == stamp) return s;
        return std::nullopt;
    };
    QList<HitZone> out;
    for (qsizetype i = 0; i + 1 < edges.size(); ++i) {
        const auto lo = edges[i], hi = edges[i + 1];
        out.append({i, lo, hi, double(lo - start) / span, double(hi - lo) / span, sampleAt(hi), sampleAt(lo)});
    }
    return out;
}
UsageGeometry usageGeometry(qint64 start, qint64 end, qint64 now,
                            const QList<QuotaSample> &samples, const QList<WindowMessage> &messages) {
    const auto hits = zones(start, end, now, samples);
    QList<qint64> weights(hits.size(), 0);
    for (const auto &m : messages) for (const auto &zone : hits) {
        if ((zone.index == 0 ? m.timestamp >= zone.loMs : m.timestamp > zone.loMs) && m.timestamp <= zone.hiMs) {
            weights[zone.index] = saturatingAdding(weights[zone.index], m.tokensExCacheRead());
            break;
        }
    }
    const auto tallest = weights.isEmpty() ? qint64(1) : std::max(*std::max_element(weights.begin(), weights.end()), qint64(1));
    QList<BarRect> bars;
    for (const auto &zone : hits) {
        const auto w = weights[zone.index];
        bars.append({zone.x, zone.width, double(w) / double(tallest), w == 0});
    }
    return {bars, hits};
}
QuotaGeometry quotaGeometry(qint64 start, qint64 end, qint64 now,
                            const QList<QuotaSample> &samples, QuotaMetric metric) {
    const double span = double(std::max(end - start, qint64(1)));
    QList<CurvePoint> points;
    for (const auto &s : samples) if (s.atMs >= start && s.atMs <= now)
        points.append({double(s.atMs - start) / span, metricValue(metric, s.usedPercent)});
    const double nowX = double(now - start) / span;
    return {points, monotoneCurve(points), nowX, points.isEmpty() ? nowX : points.first().x};
}
ChartGeometry chartGeometry(qint64 start, qint64 end, qint64 now,
                            const QList<QuotaSample> &samples, const QList<WindowMessage> &messages, QuotaMetric metric) {
    const auto usage = usageGeometry(start, end, now, samples, messages);
    const auto quota = quotaGeometry(start, end, now, samples, metric);
    return {quota.nowX, quota.firstSampleX, usage.bars, usage.hits, quota.samplePoints, quota.curve};
}
QList<CurvePoint> monotoneCurve(const QList<CurvePoint> &p) {
    const auto n = p.size();
    if (n < 2) return {};
    QList<double> dx, d, m;
    for (qsizetype i = 0; i + 1 < n; ++i) {
        const auto h = p[i + 1].x - p[i].x;
        dx.append(h);
        d.append(h == 0 ? 0 : (p[i + 1].y - p[i].y) / h);
    }
    m.append(d[0]);
    for (qsizetype i = 1; i + 1 < n; ++i) m.append(d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2);
    m.append(d[n - 2]);
    for (qsizetype i = 0; i + 1 < n; ++i) {
        if (d[i] == 0) { m[i] = 0; m[i + 1] = 0; continue; }
        const auto a = m[i] / d[i], b = m[i + 1] / d[i], s = a * a + b * b;
        if (s > 9) {
            const auto t = 3 / std::sqrt(s);
            m[i] = t * a * d[i];
            m[i + 1] = t * b * d[i];
        }
    }
    QList<CurvePoint> out{p[0]};
    for (qsizetype i = 0; i + 1 < n; ++i) {
        const auto h = dx[i];
        for (int step = 1; step <= curveResolution; ++step) {
            const auto t = double(step) / curveResolution, t2 = t * t, t3 = t2 * t;
            const auto y = (2 * t3 - 3 * t2 + 1) * p[i].y + (t3 - 2 * t2 + t) * h * m[i]
                + (-2 * t3 + 3 * t2) * p[i + 1].y + (t3 - t2) * h * m[i + 1];
            out.append({p[i].x + t * h, y});
        }
    }
    return out;
}
}
}
