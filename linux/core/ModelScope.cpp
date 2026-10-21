#include "ModelScope.h"
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTextBoundaryFinder>

namespace syrtis::core::ModelScope {
namespace {
QStringList tokens(const QString &value)
{
    const auto lower = value.toLower().normalized(QString::NormalizationForm_C);
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, lower);
    QStringList out;
    QString token;
    qsizetype start = 0;
    for (auto end = finder.toNextBoundary(); end >= 0; end = finder.toNextBoundary()) {
        const auto character = lower.mid(start, end - start);
        const auto scalars = character.toUcs4();
        if (!scalars.isEmpty() && (QChar::isLetter(scalars[0]) || QChar::isNumber(scalars[0])))
            token += character;
        else if (!token.isEmpty()) { out.append(token); token.clear(); }
        start = end;
    }
    if (!token.isEmpty()) out.append(token);
    return out;
}
}
bool covers(const QString &scope, const QString &modelId)
{
    const auto wanted = tokens(scope), model = tokens(modelId);
    if (wanted.isEmpty()) return false;
    const QSet<QString> present(model.cbegin(), model.cend());
    for (const auto &token : wanted)
        if (!present.contains(token)) return false;
    return true;
}
}
