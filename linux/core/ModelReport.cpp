#include "ModelReport.h"

namespace syrtis::core {
ModelReportEntry ModelReportEntry::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("client"),
        d.field<QString>("model"),
        d.field<QString>("provider"),
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
        d.field<qint64>("total"),
        d.field<qint64>("messageCount"),
        d.field<double>("cost"),
        d.field<std::optional<double>>("msPer1kTokens"),
        d.field<std::optional<double>>("costEstimate"),
    };
}
ModelReport ModelReport::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QList<ModelReportEntry>>("entries"),
        d.field<qint64>("totalInput"),
        d.field<qint64>("totalOutput"),
        d.field<qint64>("totalCacheRead"),
        d.field<qint64>("totalCacheWrite"),
        d.field<qint64>("totalMessages"),
        d.field<double>("totalCost"),
        d.field<std::optional<quint64>>("pricingUpdatedAt"),
    };
}
}
