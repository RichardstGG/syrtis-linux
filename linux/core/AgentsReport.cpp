#include "AgentsReport.h"

namespace syrtis::core {
AgentReportEntry AgentReportEntry::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("agent"),
        d.field<QList<QString>>("clients"),
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
        d.field<qint64>("total"),
        d.field<double>("cost"),
        d.field<qint64>("messages"),
    };
}
AgentsReport AgentsReport::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QList<AgentReportEntry>>("entries"),
        d.field<double>("totalCost"),
        d.field<qint64>("totalMessages"),
    };
}
}
