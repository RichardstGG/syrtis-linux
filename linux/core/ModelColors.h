#pragma once
#include "ModelReport.h"
#include <QtCore/QHash>

namespace syrtis::core {
namespace ModelColors {
extern const QHash<QString, QString> providerBase;
QString providerFromModel(const QString &model);
QString providerColorKey(const std::optional<QString> &providerId, const QString &modelId);
QString shadeFromBase(const QString &hex, qint64 rank);
}
struct ModelColorEntry { QString provider; QString model; double cost; };
struct ModelColorMap {
    explicit ModelColorMap(const QList<ModelColorEntry> &entries);
    explicit ModelColorMap(const std::optional<ModelReport> &report);
    QString color(const std::optional<QString> &providerId, const QString &modelId) const;
private:
    QHash<QString, QString> colorByKeyModel;
};
}
