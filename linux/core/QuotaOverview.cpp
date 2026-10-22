#include "QuotaOverview.h"
#include <algorithm>

namespace Syrtis {
QList<QuotaWindowSummary> QuotaOverviewFold::summaries(const QList<QuotaOverviewWindow> &windows)
{
    QList<QuotaWindowSummary> result;
    for (const auto &window : windows) {
        if (window.cycles.isEmpty()) continue;
        QuotaWindowSummary summary{window.clientId, window.accountKey, window.cardId,
            window.label, {}, {}, window.cycles.first().peakUsedPercent, false, window.cycles.size()};
        // Input is newest first; the most recent sixteen read oldest first.
        for (auto i = std::min(stripLength, window.cycles.size()); i > 0; --i) {
            summary.recent.append(window.cycles[i - 1].usedPercent);
            summary.recentPeaks.append(window.cycles[i - 1].peakUsedPercent);
        }
        for (const auto &cycle : window.cycles)
            summary.peakPercent = std::max(summary.peakPercent, cycle.peakUsedPercent);
        summary.neverExhausted = summary.peakPercent < exhaustedPercent;
        result.append(summary);
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.peakPercent > b.peakPercent;
    });
    return result;
}
}
