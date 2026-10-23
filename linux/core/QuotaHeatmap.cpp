#include "QuotaHeatmap.h"

#include <QtCore/QDateTime>

#include <algorithm>
#include <map>
#include <set>

namespace syrtis::core {
QuotaHeatmap QuotaHeatmapFold::build(const std::vector<QuotaCurvePoint> &points,
                                   const QTimeZone &timeZone)
{
    QuotaHeatmap result;
    std::set<QDate> days;
    std::map<qint64, std::vector<QuotaCurvePoint>> cycles;
    for (const auto &point : points)
        cycles[point.resetAt].push_back(point);

    for (auto &[resetAt, cycle] : cycles) {
        std::stable_sort(cycle.begin(), cycle.end(), [](const auto &a, const auto &b) {
            return a.sampledAt < b.sampledAt;
        });
        for (const auto &point : cycle)
            days.insert(QDateTime::fromSecsSinceEpoch(point.sampledAt, timeZone).date());
        for (size_t i = 1; i < cycle.size(); ++i) {
            const auto &previous = cycle[i - 1];
            const auto &current = cycle[i];
            const double delta = current.usedPercent - previous.usedPercent;
            if (!(delta > 0))
                continue;
            const qint64 span = current.sampledAt - previous.sampledAt;
            if (span <= 0)
                continue;
            if (span > maximumGapSeconds) {
                result.unplacedPercent += delta;
                continue;
            }
            auto cursor = previous.sampledAt;
            while (cursor < current.sampledAt) {
                const auto date = QDateTime::fromSecsSinceEpoch(cursor, timeZone);
                // Advance in elapsed seconds, preserving the particular occurrence
                // of a repeated local hour when daylight saving time ends.
                const auto time = date.time();
                const qint64 hourEnd = cursor + 3600 - time.minute() * 60 - time.second();
                const auto segmentEnd = std::min(std::max(hourEnd, cursor + 1), current.sampledAt);
                result.cells[date.date().dayOfWeek() - 1][time.hour()] +=
                    delta * double(segmentEnd - cursor) / double(span);
                cursor = segmentEnd;
            }
        }
    }

    for (const auto &row : result.cells) {
        for (double cell : row) {
            result.peak = std::max(result.peak, cell);
            result.total += cell;
        }
    }
    result.observedDays = days.size();
    return result;
}
}
