#include "QuotaCurve.h"

#include <set>
#include <tuple>

namespace Syrtis {
namespace {
using syrtis::core::json::Decoder;
void require(bool condition, const Decoder &d)
{
    if (!condition) d.fail("dataCorrupted");
}
}
QuotaCurvePoint QuotaCurvePoint::decode(const Decoder &d)
{
    d.object();
    const auto sampledAt = d.field<qint64>("sampledAt");
    const auto usedPercent = d.field<double>("usedPercent");
    const auto resetAt = d.field<qint64>("resetAt");
    const auto duration = d.field<qint64>("durationSeconds");
    const auto source = d.field<QString>("durationSource");
    QuotaCurveDurationSource durationSource;
    if (source == "provider") durationSource = QuotaCurveDurationSource::provider;
    else if (source == "contract") durationSource = QuotaCurveDurationSource::contract;
    else if (source == "observed") durationSource = QuotaCurveDurationSource::observed;
    else Decoder{d.value.toObject()["durationSource"], d.path + QStringList{"durationSource"}}.fail("dataCorrupted");
    const auto rawOrigin = d.field<QString>("origin");
    QuotaCurveSampleOrigin origin;
    if (rawOrigin == "liveV3") origin = QuotaCurveSampleOrigin::liveV3;
    else if (rawOrigin == "importedV2") origin = QuotaCurveSampleOrigin::importedV2;
    else Decoder{d.value.toObject()["origin"], d.path + QStringList{"origin"}}.fail("dataCorrupted");
    require(d.value.toObject().contains("isActiveGroup"), d);
    const auto active = d.field<bool>("isActiveGroup");
    require(duration >= 1 && duration <= QuotaCurve::maxDurationSeconds, d);
    require(std::isfinite(usedPercent) && usedPercent >= 0 && usedPercent <= 100, d);
    // Check before subtracting, matching subtractingReportingOverflow in Swift.
    require(resetAt >= std::numeric_limits<qint64>::min() + duration, d);
    require(sampledAt >= resetAt - duration && sampledAt <= resetAt, d);
    return {sampledAt, usedPercent, resetAt, duration, durationSource, origin, active};
}
QuotaCurveCoverage QuotaCurveCoverage::decode(const Decoder &d)
{
    d.object();
    return {d.field<qint64>("oldestSampledAt"), d.field<qint64>("newestSampledAt"), d.field<qint64>("sampleCount")};
}
QuotaCurve QuotaCurve::decode(const Decoder &d)
{
    d.object();
    const auto points = d.field<QList<QuotaCurvePoint>>("points");
    const auto coverage = d.field<QuotaCurveCoverage>("coverage");
    require(d.value.toObject().contains("activeResetAt"), d);
    const auto active = d.field<std::optional<qint64>>("activeResetAt");
    const auto generation = d.field<quint64>("generation");
    require(!points.isEmpty(), d);
    require(coverage.sampleCount == points.size(), d);
    require(coverage.oldestSampledAt == points.first().sampledAt
            && coverage.newestSampledAt == points.last().sampledAt, d);
    for (qsizetype i = 1; i < points.size(); ++i)
        require(points[i - 1].sampledAt <= points[i].sampledAt, d);
    std::set<std::tuple<qint64, qint64, qint64>> seen;
    for (const auto &point : points)
        require(seen.emplace(point.sampledAt, point.resetAt, point.durationSeconds).second, d);
    require(points.size() <= maxPoints, d);
    return {points, coverage, active, generation};
}
}
