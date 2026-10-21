#include "Format.h"
#include "Localization.h"
#include "UsagePace.h"

#include <QtCore/QJsonArray>
#include <QtCore/QLocale>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>

#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace Syrtis {
namespace {
QString fixed(double value, int precision)
{
    // Round the actual binary Double directly, as Swift's %.nf does; an
    // intermediate decimal rounding changes the near-halfway USD cases.
    // to_chars also keeps the English decimal point independent of LC_NUMERIC.
    std::array<char, 512> buffer;
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                      value, std::chars_format::fixed, precision);
    if (result.ec != std::errc{}) throw std::runtime_error("cannot format number");
    return QString::fromLatin1(buffer.data(), result.ptr - buffer.data());
}

QJsonObject todayEntry(const QJsonObject &graph, const QDateTime &now)
{
    const auto key = Format::todayKey(now);
    const auto contributions = graph["contributions"].toArray();
    for (auto it = contributions.constEnd(); it != contributions.constBegin();) {
        const auto entry = (*--it).toObject();
        if (entry["date"].toString() == key) return entry;
    }
    return {};
}

qint64 saturatingAdd(qint64 lhs, qint64 rhs)
{
    constexpr auto max = std::numeric_limits<qint64>::max();
    constexpr auto min = std::numeric_limits<qint64>::min();
    if (rhs > 0 && lhs > max - rhs) return max;
    if (rhs < 0 && lhs < min - rhs) return min;
    return lhs + rhs;
}
}

QString Format::compactTokens(qint64 count)
{
    const auto value = double(count);
    double scaled;
    QString suffix;
    if (value >= 999500000) {
        scaled = value / 1000000000;
        suffix = QStringLiteral("B");
    } else if (value >= 999500) {
        scaled = value / 1000000;
        suffix = QStringLiteral("M");
    } else if (value >= 1000) {
        scaled = value / 1000;
        suffix = QStringLiteral("K");
    } else {
        return QString::number(count);
    }
    auto text = fixed(scaled, scaled >= 100 ? 0 : 1);
    if (text.endsWith(QLatin1String(".0"))) text.chop(2);
    return text + suffix;
}

QString Format::exactTokens(qint64 count)
{
    return QLocale(QLocale::English, QLocale::UnitedStates).toString(count);
}

QString Format::usd(double amount)
{
    return QLatin1Char('$') + fixed(amount, 2);
}

QString Format::monthDay(const QString &iso)
{
    // Swift splits omitting empty fields, then compactMaps Int conversions.
    static const QRegularExpression integer(QStringLiteral("\\A[+-]?[0-9]+\\z"));
    QList<qint64> parts;
    for (const auto &part : iso.split(QLatin1Char('-'), Qt::SkipEmptyParts)) {
        bool ok = false;
        const auto value = part.toLongLong(&ok);
        if (ok && integer.match(part).hasMatch()) parts.append(value);
    }
    if (parts.size() != 3 || parts[1] < 1 || parts[1] > 12) return iso;
    static const QStringList months{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return localizedDefault(QStringLiteral("format.monthDay"), QStringLiteral("%1$@ %2$lld"),
                            {localized(months[parts[1] - 1]), QString::number(parts[2])});
}

QString Format::mmdd(const QString &iso)
{
    const auto parts = iso.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    return parts.size() == 3 ? parts[1] + QLatin1Char('/') + parts[2] : iso;
}

QString Format::relativeTime(quint64 epochSecs, const QDateTime &now)
{
    // Foundation truncates the current Unix seconds toward zero before the
    // subtraction, and clamps a future timestamp to "just now".
    const auto current = now.toMSecsSinceEpoch() / 1000;
    if (current <= 0 || epochSecs >= quint64(current)) return localized(QStringLiteral("just now"));
    const auto diff = quint64(current) - epochSecs;
    if (diff < 60) return localized(QStringLiteral("just now"));
    if (diff < 3600) return localized(QStringLiteral("%lldm ago"), {QString::number(diff / 60)});
    if (diff < 86400) return localized(QStringLiteral("%lldh ago"), {QString::number(diff / 3600)});
    return localized(QStringLiteral("%lldd ago"), {QString::number(diff / 86400)});
}

QString Format::todayKey(const QDateTime &now, const QTimeZone &timeZone)
{
    return now.toTimeZone(timeZone).date().toString(QStringLiteral("yyyy-MM-dd"));
}

qint64 Format::todayTokens(const QJsonObject &graph, const QDateTime &now)
{
    return todayEntry(graph, now)["totals"].toObject()["tokens"].toInteger();
}

double Format::todayCost(const QJsonObject &graph, const QDateTime &now)
{
    return todayEntry(graph, now)["totals"].toObject()["cost"].toDouble();
}

std::optional<QString> Format::todayTopClient(const QJsonObject &graph, const QDateTime &now)
{
    struct Total { qint64 tokens = 0; double cost = 0; };
    QMap<QString, Total> byClient;
    for (const auto &item : todayEntry(graph, now)["clients"].toArray()) {
        const auto client = item.toObject();
        const auto lanes = client["tokens"].toObject();
        qint64 tokens = 0;
        for (const auto *lane : {"input", "output", "cacheRead", "cacheWrite", "reasoning"})
            tokens = saturatingAdd(tokens, lanes[QLatin1String(lane)].toInteger());
        auto &total = byClient[client["client"].toString()];
        total.tokens = saturatingAdd(total.tokens, tokens);
        total.cost += client["cost"].toDouble();
    }
    std::optional<QString> best;
    Total current;
    // Sorted IDs and strict comparisons reproduce Swift's deterministic tie.
    for (auto it = byClient.cbegin(); it != byClient.cend(); ++it) {
        if (!best || it->tokens > current.tokens
            || (it->tokens == current.tokens && it->cost > current.cost)) {
            best = it.key();
            current = it.value();
        }
    }
    return best;
}

QString Format::paceDurationText(double seconds)
{
    return UsagePace::durationText(seconds);
}
}
