#include "QuotaHistory.h"
#include "WindowEquivalence.h"
#include "UsageAttributionSettings.h"
#include "SubscriptionTrend.h"
#include <iostream>

using namespace syrtis::core;
int main() {
    auto check = [](bool condition, const char *label) {
        if (!condition) { std::cerr << label << '\n'; std::exit(1); }
    };
    constexpr auto hi = std::numeric_limits<qint64>::max(), lo = std::numeric_limits<qint64>::min();
    check(saturatingAdding(hi, 1) == hi && saturatingAdding(lo, -1) == lo, "signed saturation");
    WindowMessage message;
    message.input = hi;
    message.cacheRead = hi;
    check(message.tokens() == hi && message.tokensExCacheRead() == hi, "cache read is never subtracted from saturated totals");
    message.input = lo;
    message.output = -1;
    message.cacheRead = 0;
    check(message.tokens() == hi, "WindowMessage overflow clamps to max in both directions");
    check(TokenBreakdown{lo, -1}.total() == lo, "TokenBreakdown keeps signed saturation");
    check(WindowEquivalence::clamped(std::numeric_limits<double>::infinity()) == 0
        && WindowEquivalence::clamped(std::numeric_limits<double>::quiet_NaN()) == 0
        && WindowEquivalence::clamped(double(hi)) == hi
        && WindowEquivalence::clamped(double(lo)) == lo, "ratio integer bounds");
    WindowEquivalence::Row a{WindowEquivalence::Row::Kind::ratio};
    a.tokensPerTenth = 100; a.costPerTenth = 2; a.errorPercent = 5;
    auto b = a;
    b.deltaPercent = 20; // Not an associated value of the ratio case.
    check(a == b, "enum equality uses only active payload");
    ++b.tokensPerTenth;
    check(!(a == b), "enum equality compares active payload");
    const auto nfc = QString::fromUtf8("Café"), nfd = QString::fromUtf8("Cafe\xcc\x81");
    UsageAttribution::Record r{nfc, "provider", std::nullopt, {UsageAttribution::State::Kind::assigned, "claude"}};
    auto other = r;
    other.client = nfd;
    check(r == other, "canonical source equality");
    check(UsageAttribution::resolve(nfd, "provider", "model", {r}) == r.state, "canonical source resolution");
    check(QuotaHistoryFold::canonicalReset(18089, 18000) == 18000
        && QuotaHistoryFold::canonicalReset(18090, 18000) == 18180, "reset half-up quantum");
    UsageAttributionSettings::Row sourceA{nfc, "p", 3, 1, {}, std::nullopt}, sourceB = sourceA;
    sourceB.client = nfd;
    check(sourceA == sourceB, "settings row canonical equality");
    AttributedDailySeries::Point pointA{"2026-01-01", {}, nfc, 3, 1}, pointB = pointA;
    pointB.model = nfd;
    check(pointA == pointB, "daily point canonical equality");
    check(WindowUsage::SubscriptionTotal{nfc, 3, 1} == WindowUsage::SubscriptionTotal{nfd, 3, 1}, "window total canonical equality");
    SubscriptionTrend::Day dayA{"2026-01-01", {{nfc, {3, 1}}}, 3, 1}, dayB = dayA;
    dayB.byTarget = {{nfd, {3, 1}}};
    check(dayA == dayB, "trend dictionary canonical equality");
    const auto trend = SubscriptionTrendFold::build({
        {"2026-01-01", {UsageAttribution::State::Kind::assigned, "z"}, "m", 10, 1},
        {"2026-01-01", {UsageAttribution::State::Kind::assigned, "a"}, "m", 10, 1}}, "2026-01-01", 1);
    check(trend.targets == QStringList{"a", "z"} && trend.targetsByTokens == trend.targets, "trend metric ties use target name");
    auto trendA = trend, trendB = trend;
    trendA.targets = {nfc}; trendB.targets = {nfd};
    check(trendA == trendB, "trend target canonical equality");
    check(SubscriptionTrendFold::build({}, "2026-01-01", -1) == SubscriptionTrend::empty, "negative day count is empty");
    ModelReportEntry positiveZero{}, negativeZero{};
    negativeZero.cost = -0.0;
    check(UsageAttributionSettings::signature({positiveZero}, {}) != UsageAttributionSettings::signature({negativeZero}, {}), "signature preserves cost bit pattern");
    std::cout << "attribution value checks OK\n";
}
