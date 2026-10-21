#include "HourlyReport.h"

namespace syrtis::core {
HourlyReportEntry HourlyReportEntry::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("hour"),
        d.field<QList<QString>>("clients"),
        d.field<QList<QString>>("models"),
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
        d.field<qint64>("total"),
        d.field<qint64>("messageCount"),
        d.field<qint64>("turnCount"),
        d.field<double>("cost"),
    };
}
HourlyReport HourlyReport::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QList<HourlyReportEntry>>("entries"),
        d.field<double>("totalCost"),
    };
}
}
