#include "UsagePace.h"
#include "Localization.h"

#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>

#include <algorithm>
#include <cmath>

namespace syrtis::core {
namespace {
PaceStage stageFor(double delta)
{
    const auto a = std::abs(delta);
    if (a <= 2) return PaceStage::onTrack;
    if (a <= 6) return delta >= 0 ? PaceStage::slightlyAhead : PaceStage::slightlyBehind;
    if (a <= 12) return delta >= 0 ? PaceStage::ahead : PaceStage::behind;
    return delta >= 0 ? PaceStage::farAhead : PaceStage::farBehind;
}

bool isDurationReady(UsagePaceState state)
{
    return state == UsagePaceState::learningHistory || state == UsagePaceState::available;
}
}

bool isDeficit(PaceStage stage)
{
    return stage == PaceStage::slightlyAhead || stage == PaceStage::ahead || stage == PaceStage::farAhead;
}

std::optional<double> parseRFC3339(const QString &text)
{
    // QDateTime's ISO parser also accepts date-only input; Foundation's
    // withInternetDateTime contract requires a time and timezone.
    static const QRegularExpression shape(QStringLiteral(
        "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(\\.\\d+)?(Z|[+-]\\d{2}:\\d{2})$"));
    if (!shape.match(text).hasMatch()) return std::nullopt;
    const auto date = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!date.isValid()) return std::nullopt;
    return double(date.toMSecsSinceEpoch()) / 1000.0 - 978307200.0;
}

bool UsagePace::isHistoricalDeficit() const
{
    return basis == UsagePaceBasis::historical && isDeficit(stage);
}

QString UsagePace::label() const
{
    if (stage == PaceStage::onTrack) return localized(QStringLiteral("On pace"));
    const auto d = QString::number(std::round(std::abs(deltaPercent)), 'f', 0);
    return localized(isDeficit(stage) ? QStringLiteral("%lld%% in deficit")
                                      : QStringLiteral("%lld%% in reserve"), {d});
}

std::optional<QString> UsagePace::etaText() const
{
    if (willLastToReset)
        return localized(basis == UsagePaceBasis::historical
            ? QStringLiteral("Historically: lasts until reset")
            : QStringLiteral("On average: lasts until reset"));
    if (!etaSeconds) return std::nullopt;
    const auto text = durationText(*etaSeconds);
    return text == nowText() ? localized(QStringLiteral("Projected empty now"))
        : localized(QStringLiteral("Projected empty in %@"), {text});
}

QString UsagePace::nowText()
{
    return localizedDefault(QStringLiteral("duration.now"), QStringLiteral("now"));
}

QString UsagePace::durationText(double seconds)
{
    const auto minutes = std::round(seconds / 60);
    if (minutes < 1) return nowText();
    const auto m = static_cast<qint64>(minutes);
    if (m < 60) return localized(QStringLiteral("%lldm"), {QString::number(m)});
    const auto h = m / 60;
    const auto rem = m % 60;
    if (h < 24) {
        return rem > 0 ? localized(QStringLiteral("%lldh %lldm"), {QString::number(h), QString::number(rem)})
                       : localized(QStringLiteral("%lldh"), {QString::number(h)});
    }
    const auto days = h / 24;
    const auto hr = h % 24;
    return hr > 0 ? localized(QStringLiteral("%lldd %lldh"), {QString::number(days), QString::number(hr)})
                  : localized(QStringLiteral("%lldd"), {QString::number(days)});
}

std::optional<QString> UsagePace::spanText(double reset, double now)
{
    const auto seconds = std::floor(reset - now);
    if (seconds <= 0) return std::nullopt;
    const auto minutes = static_cast<qint64>((seconds + 59) / 60);
    return durationText(double(minutes * 60));
}

std::optional<QString> UsagePace::resetText(const QString &resetsAt, double now)
{
    const auto reset = parseRFC3339(resetsAt);
    if (!reset) return std::nullopt;
    const auto span = spanText(*reset, now);
    return span ? localized(QStringLiteral("Resets in %@"), {*span})
                : localized(QStringLiteral("Resets now"));
}

std::optional<UsagePace> UsagePace::compute(const UsageWindow &window, double now)
{
    return compute(window, PaceMode::linear, now);
}

std::optional<UsagePace> UsagePace::compute(const UsageWindow &window, PaceMode mode, double now)
{
    if (mode == PaceMode::off) return std::nullopt;
    const auto state = window.paceStatus.state;
    if (!isDurationReady(state)) return std::nullopt;
    const bool historical = mode == PaceMode::historical && state == UsagePaceState::available;
    if (historical && !window.historicalPace) return std::nullopt;
    if (!window.resetsAt || !window.paceStatus.durationSeconds
        || *window.paceStatus.durationSeconds <= 0) return std::nullopt;
    const auto reset = parseRFC3339(*window.resetsAt);
    if (!reset) return std::nullopt;
    const auto duration = double(*window.paceStatus.durationSeconds);
    const auto untilReset = *reset - now;
    if (untilReset <= 0 || untilReset > duration) return std::nullopt;
    const auto elapsed = std::clamp(duration - untilReset, 0.0, duration);
    const auto actual = std::clamp(window.usedPercent, 0.0, 100.0);
    if (elapsed == 0 && actual > 0) return std::nullopt;
    const auto expected = std::clamp(historical ? window.historicalPace->expectedUsedPercent
                                               : elapsed / duration * 100, 0.0, 100.0);
    const auto delta = actual - expected;
    std::optional<double> eta;
    bool willLast = false;
    if (historical) {
        eta = window.historicalPace->etaSeconds;
        willLast = window.historicalPace->willLastToReset;
    } else if (elapsed > 0 && actual > 0) {
        const auto rate = actual / elapsed;
        if (rate > 0) {
            const auto candidate = std::max(0.0, 100 - actual) / rate;
            if (candidate >= untilReset) willLast = true;
            else eta = candidate;
        }
    } else if (elapsed > 0 && actual == 0) {
        willLast = true;
    }
    return UsagePace{stageFor(delta), historical ? UsagePaceBasis::historical : UsagePaceBasis::linear,
                     delta, expected, actual, eta, willLast};
}

UsagePacePresentation UsagePace::presentation(const UsageWindow &window, PaceMode mode, const UsagePace &pace)
{
    const auto risk = mode == PaceMode::historical ? runOutRiskLabel(window, pace) : std::nullopt;
    return {pace.willLastToReset && risk ? std::nullopt : pace.etaText(), risk};
}

std::optional<QString> runOutRiskLabel(const UsageWindow &window, const std::optional<UsagePace> &pace)
{
    if (window.paceStatus.state != UsagePaceState::available
        || (pace && pace->basis == UsagePaceBasis::linear)
        || !window.historicalPace || !window.historicalPace->runOutProbability) return std::nullopt;
    const auto pct = std::round(std::clamp(*window.historicalPace->runOutProbability, 0.0, 1.0) * 100);
    if (pct <= 0) return std::nullopt;
    return localized(QStringLiteral("≈ %lld%% run-out risk"), {QString::number(pct, 'f', 0)});
}
}
