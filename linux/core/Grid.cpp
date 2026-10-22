#include "Grid.h"
#include <algorithm>

namespace syrtis::core {
std::optional<ISODay> ISODay::parse(const QString &iso)
{
    QList<qint64> parts;
    for (const auto &part : iso.split('-', Qt::SkipEmptyParts)) {
        bool ok = false;
        // Swift Int(String) does not trim whitespace as QString::toLongLong does.
        const auto value = part.toLongLong(&ok);
        if (ok && part == part.trimmed()) parts.append(value);
    }
    if (parts.size() != 3) return std::nullopt;
    const auto y = parts[0], m = parts[1], d = parts[2];
    const auto yy = m <= 2 ? y - 1 : y;
    const auto era = (yy >= 0 ? yy : yy - 399) / 400;
    const auto yoe = yy - era * 400;
    const auto doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return ISODay{era * 146097 + doe - 719468};
}
QString ISODay::iso() const
{
    const auto z = number + 719468;
    const auto era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = z - era * 146097;
    const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const auto mp = (5 * doy + 2) / 153;
    const auto d = doy - (153 * mp + 2) / 5 + 1;
    const auto m = mp + (mp < 10 ? 3 : -9);
    const auto y = yoe + era * 400 + (m <= 2 ? 1 : 0);
    return QString::asprintf("%04d-%02d-%02d", int(y), int(m), int(d));
}
int ISODay::weekday() const { return int(((number + 4) % 7 + 7) % 7); }
GridLayout buildGrid(const QString &year, const QHash<QString, PerDay> &perDayMap)
{
    const auto jan1 = ISODay::parse(year + QStringLiteral("-01-01"));
    const auto dec31 = ISODay::parse(year + QStringLiteral("-12-31"));
    if (!jan1 || !dec31) return {53, 7, {}, 0};
    const ISODay start{jan1->number - jan1->weekday()};
    const auto lastCol = (dec31->number - start.number) / 7;
    const auto cols = int(std::max(qint64(53), lastCol + 1));
    GridLayout grid{cols, 7, {}, 0};
    grid.cells.reserve(cols * 7);
    for (int col = 0; col < cols; ++col) {
        for (int row = 0; row < 7; ++row) {
            const auto date = ISODay{start.number + col * 7 + row}.iso();
            const bool inYear = date.startsWith(year + '-');
            const auto entry = perDayMap.value(date);
            const bool active = inYear && entry.tokens > 0;
            if (active) grid.maxTokens = std::max(grid.maxTokens, entry.tokens);
            grid.cells.append({col, row, date, inYear, active, entry.tokens, entry.cost});
        }
    }
    return grid;
}
}
