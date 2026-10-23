#include "UsagePace.h"
#include "Localization.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace syrtis::core;

namespace {
void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected)
{
    check(std::abs(actual - expected) <= 1e-9, "pace math drift");
}

UsageWindow sample()
{
    UsageWindow window;
    window.usedPercent = 50;
    window.remainingPercent = 50;
    window.resetsAt = QStringLiteral("2026-07-10T12:50:00Z");
    window.paceStatus.state = UsagePaceState::learningHistory;
    window.paceStatus.durationSeconds = 6000;
    return window;
}
}

int main()
{
    try {
        const auto now = *parseRFC3339(QStringLiteral("2026-07-10T12:00:00Z"));
        auto window = sample();
        auto pace = UsagePace::compute(window, now);
        check(pace && pace->stage == PaceStage::onTrack && pace->willLastToReset && !pace->etaSeconds,
              "candidate equal to reset must last");
        near(pace->expectedUsedPercent, 50);
        check(pace->etaText() == QStringLiteral("On average: lasts until reset"), "linear lasts text");
        check(!UsagePace::compute(window, PaceMode::off, now), "off mode");

        for (const auto state : {UsagePaceState::legacyMissing, UsagePaceState::learningDuration,
                                  UsagePaceState::unavailable}) {
            window.paceStatus.state = state;
            window.windowMinutes = 100;
            check(!UsagePace::compute(window, now), "no linear revival from legacy minutes");
            check(!UsagePace::compute(window, PaceMode::historical, now), "no historical fallback");
        }
        window = sample();
        check(UsagePace::compute(window, PaceMode::historical, now)->basis == UsagePaceBasis::linear,
              "learningHistory explicitly uses linear");
        struct StageCase { double delta; PaceStage stage; };
        for (const auto &[delta, stage] : {
                 StageCase{2, PaceStage::onTrack}, {-2, PaceStage::onTrack},
                 {2.5, PaceStage::slightlyAhead}, {6, PaceStage::slightlyAhead},
                 {6.5, PaceStage::ahead}, {12, PaceStage::ahead}, {12.5, PaceStage::farAhead},
                 {-2.5, PaceStage::slightlyBehind}, {-6, PaceStage::slightlyBehind},
                 {-6.5, PaceStage::behind}, {-12, PaceStage::behind}, {-12.5, PaceStage::farBehind}}) {
            window.usedPercent = 50 + delta;
            pace = UsagePace::compute(window, now);
            check(pace && pace->stage == stage, "stage inclusive thresholds");
            near(pace->deltaPercent, delta);
        }
        window.usedPercent = 52.5;
        check(UsagePace::compute(window, now)->label() == QStringLiteral("3% in deficit"), "positive half rounding");
        window.usedPercent = 47.5;
        check(UsagePace::compute(window, now)->label() == QStringLiteral("3% in reserve"), "negative half rounding");
        window.usedPercent = 60;
        pace = UsagePace::compute(window, now);
        near(*pace->etaSeconds, 2000);
        check(pace->etaText() == QStringLiteral("Projected empty in 33m"), "linear ETA text");
        window.usedPercent = 250;
        pace = UsagePace::compute(window, now);
        near(pace->actualUsedPercent, 100);
        near(*pace->etaSeconds, 0);
        check(pace->etaText() == QStringLiteral("Projected empty now"), "clamped empty ETA");
        window.usedPercent = -5;
        pace = UsagePace::compute(window, now);
        near(pace->actualUsedPercent, 0);
        check(pace->willLastToReset, "clamped zero usage lasts");

        window = sample();
        for (const auto &reset : {"2026-07-10T11:59:59Z", "2026-07-10T12:00:00Z",
                                 "2026-07-10T13:40:01Z", "2026-07-10", "bad"}) {
            window.resetsAt = QString::fromLatin1(reset);
            check(!UsagePace::compute(window, now), "timing rejection");
        }
        window.resetsAt.reset();
        check(!UsagePace::compute(window, now), "missing reset");
        window = sample();
        for (const auto duration : {0, -1}) {
            window.paceStatus.durationSeconds = duration;
            check(!UsagePace::compute(window, now), "nonpositive duration");
        }
        window.paceStatus.durationSeconds.reset();
        check(!UsagePace::compute(window, now), "missing duration");
        window = sample();
        window.resetsAt = QStringLiteral("2026-07-10T13:40:00Z");
        check(!UsagePace::compute(window, now), "start with usage rejected");
        window.usedPercent = 0;
        pace = UsagePace::compute(window, now);
        check(pace && !pace->willLastToReset && !pace->etaText(), "start at zero has no projection");
        window = sample();
        window.resetsAt = QStringLiteral("2026-07-10T20:50:00+08:00");
        near(UsagePace::compute(window, now)->expectedUsedPercent, 50);
        window.resetsAt = QStringLiteral("2026-07-10T12:50:00.500Z");
        near(UsagePace::compute(window, now)->expectedUsedPercent, 2999.5 / 6000 * 100);
        window.resetsAt = QStringLiteral("2026-07-15T12:00:00Z");
        window.paceStatus.durationSeconds = 7 * 86400;
        window.usedPercent = 30;
        pace = UsagePace::compute(window, now);
        near(*pace->etaSeconds, 70.0 / (30.0 / (2 * 86400)));
        check(pace->etaText() == QStringLiteral("Projected empty in 4d 16h"), "multi-day ETA");

        window = sample();
        window.paceStatus.state = UsagePaceState::available;
        check(!UsagePace::compute(window, PaceMode::historical, now), "available needs backend result");
        window.historicalPace = HistoricalPace{20, std::nullopt, true, .3};
        pace = UsagePace::compute(window, PaceMode::historical, now);
        near(pace->expectedUsedPercent, 20);
        check(pace->isHistoricalDeficit(), "historical basis deficit");
        check(pace->etaText() == QStringLiteral("Historically: lasts until reset"), "historical lasts text");
        const auto display = UsagePace::presentation(window, PaceMode::historical, *pace);
        check(!display.etaText && display.riskText == QStringLiteral("≈ 30% run-out risk"), "risk takes precedence");
        auto linear = UsagePace::compute(window, now);
        check(!linear->isHistoricalDeficit() && !runOutRiskLabel(window, linear), "linear suppresses backend risk");
        check(!UsagePace::presentation(window, PaceMode::linear, *linear).riskText, "linear presentation risk");
        window.historicalPace = HistoricalPace{80, 123.5, false, std::nullopt};
        pace = UsagePace::compute(window, PaceMode::historical, now);
        near(*pace->etaSeconds, 123.5);
        near(pace->deltaPercent, -30);
        check(!pace->willLastToReset && !runOutRiskLabel(window), "backend projection preserved without risk");
        window.resetsAt = QStringLiteral("2026-07-10T13:40:00Z");
        check(!UsagePace::compute(window, PaceMode::historical, now), "historical start with usage rejected");
        window.resetsAt = QStringLiteral("2026-07-10T12:50:00Z");
        for (const auto probability : {-1.0, 0.0, .004, .005, .3, 1.0, 1.5}) {
            window.historicalPace->runOutProbability = probability;
            const auto risk = runOutRiskLabel(window);
            if (probability < .005) check(!risk, "rounded zero risk hidden");
            else check(risk == QStringLiteral("≈ %1% run-out risk").arg(
                           probability == .005 ? 1 : probability == .3 ? 30 : 100), "risk rounding and clamp");
        }
        window.paceStatus.state = UsagePaceState::learningHistory;
        check(!runOutRiskLabel(window), "risk requires available");

        for (const auto &[seconds, text] : {
                 std::pair<double, const char *>{-60, "now"}, {0, "now"}, {29.999, "now"},
                 {30, "1m"}, {89.999, "1m"}, {90, "2m"}, {3570, "1h"},
                 {3600, "1h"}, {3660, "1h 1m"}, {86370, "1d"},
                 {90000, "1d 1h"}, {176400, "2d 1h"}})
            check(UsagePace::durationText(seconds) == QLatin1String(text), "duration rounding and units");
        check(!UsagePace::spanText(now + .999, now), "countdown floors seconds first");
        check(UsagePace::spanText(now + 60.9, now) == QStringLiteral("1m"), "countdown integral minute");
        check(UsagePace::spanText(now + 61, now) == QStringLiteral("2m"), "countdown rounds minutes up");
        check(UsagePace::resetText(QStringLiteral("2026-07-10T12:00:00Z"), now) == QStringLiteral("Resets now"), "reset now");
        check(UsagePace::resetText(QStringLiteral("2026-07-10T12:50:00Z"), now) == QStringLiteral("Resets in 50m"), "reset countdown");
        check(!UsagePace::resetText(QStringLiteral("bad"), now), "invalid reset text");

        check(localized(QStringLiteral("English %%")) == QStringLiteral("English %%"), "lookup is identity");
        check(localized(QStringLiteral("%lld%% %@"), {QStringLiteral("5"), QStringLiteral("%@")})
                  == QStringLiteral("5% %@"), "format does not reinterpret arguments");
        check(localizedDefault(QStringLiteral("semantic.key"), QStringLiteral("%2$lld %1$@"),
                               {QStringLiteral("July"), QStringLiteral("10")}) == QStringLiteral("10 July"), "semantic fallback and positional format");
        std::cout << "usage-pace unit tests OK\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
