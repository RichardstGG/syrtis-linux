#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QTimeZone>

#include <optional>

namespace syrtis::core {
// English-pinned cross-checked surface of Sources/Syrtis/Format.swift.
struct Format {
    static QString compactTokens(qint64 count);
    static QString exactTokens(qint64 count);
    static QString usd(double amount);
    static QString monthDay(const QString &iso);
    static QString mmdd(const QString &iso);
    static QString relativeTime(quint64 epochSecs, const QDateTime &now = QDateTime::currentDateTime());
    static QString todayKey(const QDateTime &now = QDateTime::currentDateTime(),
                            const QTimeZone &timeZone = QTimeZone::systemTimeZone());

    // The format selector's unfiltered Graph.swift trayTotals projection.
    // Read Rust's existing day totals; duplicate dates select the last entry.
    static qint64 todayTokens(const QJsonObject &graph, const QDateTime &now = QDateTime::currentDateTime());
    static double todayCost(const QJsonObject &graph, const QDateTime &now = QDateTime::currentDateTime());
    static std::optional<QString> todayTopClient(const QJsonObject &graph,
                                                const QDateTime &now = QDateTime::currentDateTime());
    static QString paceDurationText(double seconds);
};
}
