#pragma once
#include <QtCore/QStringList>

namespace syrtis::core::OverviewCard {
QStringList allCases();
QString label(const QString &id);
QStringList toggleable();
QStringList visible(const QString &hiddenRaw);
inline const QString hiddenKey = QStringLiteral("tokenbar.overview.hidden");
}
