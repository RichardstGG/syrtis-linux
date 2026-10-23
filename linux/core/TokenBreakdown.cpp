#include "TokenBreakdown.h"

#include <limits>

namespace syrtis::core {
qint64 saturatingAdding(qint64 a, qint64 b)
{
    if (b > 0 && a > std::numeric_limits<qint64>::max() - b)
        return std::numeric_limits<qint64>::max();
    if (b < 0 && a < std::numeric_limits<qint64>::min() - b)
        return std::numeric_limits<qint64>::min();
    return a + b;
}

qint64 TokenBreakdown::total() const
{
    return saturatingAdding(saturatingAdding(saturatingAdding(
        saturatingAdding(input, output), cacheRead), cacheWrite), reasoning);
}

TokenBreakdown TokenBreakdown::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("input"),
        d.field<qint64>("output"),
        d.field<qint64>("cacheRead"),
        d.field<qint64>("cacheWrite"),
        d.field<qint64>("reasoning"),
    };
}
}
