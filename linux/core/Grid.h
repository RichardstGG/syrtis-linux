#pragma once
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <optional>

namespace syrtis::core {
// Grid's civil-date and per-day value dependencies from UsageStats.swift.
struct ISODay {
    qint64 number;
    static std::optional<ISODay> parse(const QString &iso);
    QString iso() const;
    int weekday() const;
    bool operator==(const ISODay &) const = default;
};
struct PerDay {
    QString date;
    qint64 tokens = 0;
    double cost = 0;
    int intensity = 0;
    bool hasMessages = false;
    bool isActive() const { return tokens > 0 || hasMessages; }
};
struct GridCell {
    int col;
    int row;
    QString date;
    bool inYear;
    bool active;
    qint64 tokens;
    double cost;
};
struct GridLayout {
    int cols;
    int rows;
    QList<GridCell> cells;
    qint64 maxTokens;
};
GridLayout buildGrid(const QString &year, const QHash<QString, PerDay> &perDayMap);
}
