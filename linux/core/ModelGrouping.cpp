#include "ModelGrouping.h"
#include <QtCore/QStringList>

namespace syrtis::core::ModelGrouping {
QString groupID(const QString &id)
{
    if (id.size() <= 11 || !id.startsWith(QStringLiteral("grok-"))
        || !id.endsWith(QStringLiteral("-build"))) return id;
    const auto version = id.mid(5, id.size() - 11);
    for (const auto &part : version.split('.', Qt::KeepEmptyParts)) {
        if (part.isEmpty()) return id;
        for (const auto ch : part)
            if (ch < u'0' || ch > u'9') return id;
    }
    return QStringLiteral("grok-") + version;
}
}
