#include "UsageStats.h"
#include <algorithm>

namespace syrtis::core {
Streaks Streaks::compute(const QHash<QString, PerDay> &perDayMap,
                        const QString &rangeStart, const QString &rangeEnd)
{
    const auto start = ISODay::parse(rangeStart), end = ISODay::parse(rangeEnd);
    if (!start || !end || end->number < start->number) return {};
    Streaks result;
    qint64 run = 0;
    for (auto n = start->number; n <= end->number; ++n) {
        if (perDayMap.value(ISODay{n}.iso()).isActive()) {
            ++run;
            result.longest = std::max(result.longest, run);
        } else run = 0;
    }
    for (auto n = end->number; n >= start->number; --n) {
        if (!perDayMap.value(ISODay{n}.iso()).isActive()) break;
        ++result.current;
    }
    return result;
}
UsageStats::UsageStats(const UsagePayload &payload, const QSet<QString> &selectedClients)
{
    QSet<QString> present;
    for (const auto &c : payload.contributions) {
        qint64 dayTokens = 0;
        double dayCost = 0;
        bool hasMessages = false;
        for (const auto &cc : c.clients) {
            present.insert(cc.client);
            if (!selectedClients.contains(cc.client)) continue;
            dayTokens = saturatingAdding(dayTokens, cc.tokens.total());
            dayCost += cc.cost;
            hasMessages = hasMessages || cc.messages > 0;
        }
        if (dayTokens == 0 && dayCost == 0 && !hasMessages) continue;
        const PerDay entry{c.date, dayTokens, dayCost, c.intensity, hasMessages};
        perDay.append(entry);
        perDayMap[c.date] = entry;
        totalTokens = saturatingAdding(totalTokens, dayTokens);
        totalCost += dayCost;
        maxTokens = std::max(maxTokens, dayTokens);
        if (!bestDay || dayCost > bestDay->cost) bestDay = BestDay{c.date, dayCost};
    }
    activeDays = perDay.size();
    averagePerDay = perDay.isEmpty() ? 0 : totalCost / double(perDay.size());
    dateRange = payload.meta.dateRange;
    if (!selectedClients.contains(present) && !perDay.isEmpty()) {
        QStringList dates;
        for (const auto &day : perDay) dates.append(day.date);
        const auto bounds = std::minmax_element(dates.begin(), dates.end());
        dateRange = {*bounds.first, *bounds.second};
    }
    streaks = Streaks::compute(perDayMap, dateRange.start, dateRange.end);
    presentClients = present.values();
    std::sort(presentClients.begin(), presentClients.end());
}
static bool isVisible(const ContributionClient &cc, const QSet<QString> &hidden)
{
    return !hidden.contains(cc.client) && (cc.tokens.total() > 0 || cc.cost > 0 || cc.messages > 0);
}
QSet<QString> UsageStats::yearsWithVisibleActivity(const QList<Contribution> &contributions,
                                                 const QSet<QString> &hidden)
{
    QSet<QString> years;
    for (const auto &c : contributions) {
        const auto year = c.date.left(4);
        if (years.contains(year)) continue;
        if (std::any_of(c.clients.begin(), c.clients.end(), [&](const auto &cc) { return isVisible(cc, hidden); }))
            years.insert(year);
    }
    return years;
}
bool UsageStats::hasVisibleActivity(const QList<Contribution> &contributions, const QSet<QString> &hidden)
{
    for (const auto &c : contributions)
        for (const auto &cc : c.clients) if (isVisible(cc, hidden)) return true;
    return false;
}
}
