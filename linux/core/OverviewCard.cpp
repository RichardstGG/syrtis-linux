#include "OverviewCard.h"
#include "ClientRegistry.h"

namespace syrtis::core::OverviewCard {
QStringList allCases() { return {"quotaSummary", "chart", "limits", "trace", "models", "streaks"}; }
QString label(const QString &id)
{
    QString spaced;
    for (const auto c : id) {
        if (c.isUpper() && !spaced.isEmpty()) { spaced += ' '; spaced += c.toLower(); }
        else spaced += c;
    }
    if (!spaced.isEmpty()) spaced[0] = spaced[0].toUpper();
    return spaced;
}
QStringList toggleable() { auto result = allCases(); result.removeAll("chart"); return result; }
QStringList visible(const QString &hiddenRaw)
{
    const auto hidden = ClientRegistry::parseIdSet(hiddenRaw);
    QStringList result;
    for (const auto &id : allCases()) if (id == "chart" || !hidden.contains(id)) result.append(id);
    return result;
}
}
