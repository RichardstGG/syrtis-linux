#pragma once

#include <QtCore/QStringList>

namespace syrtis::core {
// English-only counterpart of Localization.swift. Source text is its own key.
QString localized(const QString &key);
QString localized(const QString &key, const QStringList &arguments);
QString localizedDefault(const QString &key, const QString &fallback,
                         const QStringList &arguments = {});
}
