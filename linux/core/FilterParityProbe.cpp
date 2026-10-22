#include "FilterParityProbe.h"
#include <iomanip>
#include <locale>
#include <sstream>

namespace syrtis::core {
namespace {
FilterParityStatus decodeStatus(const QString &value, const QStringList &path)
{
    if (value == "match") return FilterParityStatus::match;
    if (value == "mismatch") return FilterParityStatus::mismatch;
    if (value == "sourceChanged") return FilterParityStatus::sourceChanged;
    if (value == "tokenUnavailable") return FilterParityStatus::tokenUnavailable;
    throw DecodingError("dataCorrupted", path);
}
}
FilterParityAggregate FilterParityAggregate::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("entryCount"),
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
        d.field<qint64>("totalTokens"),
        d.field<qint64>("messageCount"),
        d.field<double>("totalCost"),
    };
}
FilterParityReport FilterParityReport::decode(const json::Decoder &d)
{
    d.object();
    return {
        decodeStatus(d.field<QString>("status"), d.path + QStringList{"status"}),
        d.field<std::optional<FilterParityAggregate>>("unfiltered"),
        d.field<std::optional<FilterParityAggregate>>("full"),
        d.field<std::optional<FilterParityAggregate>>("delta"),
    };
}
FilterParityProbe FilterParityProbe::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<FilterParityReport>("hourly"),
        d.field<FilterParityReport>("agents"),
        d.field<qint64>("presentClientCount"),
    };
}
QString smokeLabel(FilterParityStatus status)
{
    switch (status) {
    case FilterParityStatus::match: return "MATCH";
    case FilterParityStatus::mismatch: return "MISMATCH";
    case FilterParityStatus::sourceChanged: return "SOURCE_CHANGED / SKIP";
    case FilterParityStatus::tokenUnavailable: return "TOKEN_UNAVAILABLE / SKIP";
    }
    return {};
}
QString FilterParityReport::smokeSummary() const
{
    const auto label = smokeLabel(status);
    if (!delta || status == FilterParityStatus::sourceChanged
        || status == FilterParityStatus::tokenUnavailable) return label;
    std::ostringstream cost;
    cost.imbue(std::locale::classic());
    cost << std::fixed << std::setprecision(2) << delta->totalCost;
    return QString("%1 entriesΔ=%2 tokensΔ=%3 messagesΔ=%4 costΔ=%5")
        .arg(label).arg(delta->entryCount).arg(delta->totalTokens)
        .arg(delta->messageCount).arg(QString::fromStdString(cost.str()));
}
QString FilterParityProbe::smokeSummary() const
{
    return QString("hourly=%1; agents=%2").arg(hourly.smokeSummary(), agents.smokeSummary());
}
}
