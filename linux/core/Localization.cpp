#include "Localization.h"

#include <QtCore/QRegularExpression>

namespace Syrtis {
QString localized(const QString &key)
{
    return key;
}

QString localized(const QString &key, const QStringList &arguments)
{
    // String and integer placeholders used by the core's English copy. Replace
    // in one pass so a '%' inside an argument is never interpreted as a format.
    static const QRegularExpression token(QStringLiteral("%((\\d+)\\$)?(@|lld|%)"));
    QString result;
    qsizetype offset = 0;
    qsizetype argument = 0;
    auto matches = token.globalMatch(key);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += key.mid(offset, match.capturedStart() - offset);
        if (match.captured(3) == QStringLiteral("%")) {
            result += QLatin1Char('%');
        } else {
            const auto index = match.captured(2).isEmpty()
                ? argument++ : match.captured(2).toLongLong() - 1;
            result += arguments.value(index);
        }
        offset = match.capturedEnd();
    }
    return result + key.mid(offset);
}

QString localizedDefault(const QString &, const QString &fallback, const QStringList &arguments)
{
    return localized(fallback, arguments);
}
}
