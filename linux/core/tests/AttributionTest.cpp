#include "QuotaHistory.h"
#include "WindowEquivalence.h"
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
    std::cout << "attribution value checks OK\n";
}
