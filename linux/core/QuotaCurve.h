#pragma once
#include "JsonDecoder.h"

namespace Syrtis {
enum class QuotaCurveDurationSource { provider, contract, observed };
enum class QuotaCurveSampleOrigin { liveV3, importedV2 };
struct QuotaCurvePoint {
    qint64 sampledAt;
    double usedPercent;
    qint64 resetAt, durationSeconds;
    QuotaCurveDurationSource durationSource;
    QuotaCurveSampleOrigin origin;
    bool isActiveGroup;
    bool operator==(const QuotaCurvePoint &) const = default;
    static QuotaCurvePoint decode(const syrtis::core::json::Decoder &decoder);
};
struct QuotaCurveCoverage {
    qint64 oldestSampledAt, newestSampledAt, sampleCount;
    bool operator==(const QuotaCurveCoverage &) const = default;
    static QuotaCurveCoverage decode(const syrtis::core::json::Decoder &decoder);
};
struct QuotaCurve {
    QList<QuotaCurvePoint> points;
    QuotaCurveCoverage coverage;
    std::optional<qint64> activeResetAt;
    quint64 generation;
    static constexpr qint64 maxDurationSeconds = 400 * 86400;
    static constexpr qsizetype maxPoints = 65536;
    bool operator==(const QuotaCurve &) const = default;
    static QuotaCurve decode(const syrtis::core::json::Decoder &decoder);
};
}
