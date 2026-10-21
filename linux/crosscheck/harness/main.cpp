#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QtCore/QSaveFile>
#include <QtCore/QStringList>

#include <iostream>

namespace {
int fail(const QString &message, int code)
{
    std::cerr << message.toStdString() << '\n';
    return code;
}
}

int main(int argc, char *argv[])
{
    if (qgetenv("TZ") != "Asia/Taipei")
        return fail(QStringLiteral("error: TZ must be Asia/Taipei"), 1);

    QStringList args;
    QString appleLanguages;
    bool hasLanguageOption = false;
    for (int i = 1; i < argc; ++i) {
        const auto token = QString::fromLocal8Bit(argv[i]);
        // Match Swift's option scanning, including the single '-' positional
        // token and last-value-wins handling of -AppleLanguages.
        if (!token.startsWith('-') || token.size() == 1) {
            args.append(token);
            continue;
        }
        if (token != QStringLiteral("-AppleLanguages") || i + 1 >= argc)
            return fail(QStringLiteral("error: unsupported option %1; only -AppleLanguages <value> is recognized")
                            .arg(token), 2);
        hasLanguageOption = true;
        appleLanguages = QString::fromLocal8Bit(argv[++i]);
    }
    if (hasLanguageOption && appleLanguages != QStringLiteral("(en)"))
        return fail(QStringLiteral("error: language must be pinned to (en)"), 1);
    // The three-position Linux CLI needs no NSUserDefaults option. Pin the
    // equivalent locale here, independent of the invoking host's locale.
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    if (args.size() < 2 || args.size() > 3)
        return fail(QStringLiteral("usage: syrtis-crosscheck <fixtures-dir> <out-dir> [usage-pace|format|provider-quota-pace-v3]"), 2);

    const QStringList knownSelectors{
        QStringLiteral("usage-pace"), QStringLiteral("format"),
        QStringLiteral("provider-quota-pace-v3")};
    const auto selector = args.size() == 3 ? args[2] : QStringLiteral("all");
    if (selector != QStringLiteral("all") && !knownSelectors.contains(selector))
        return fail(QStringLiteral("error: unknown selector %1").arg(selector), 2);

    const QDir outDir(args[1]);
    if (!outDir.mkpath(QStringLiteral(".")))
        return fail(QStringLiteral("error: cannot create output directory"), 1);

    const auto selected = selector == QStringLiteral("all")
        ? QStringList{QStringLiteral("usage-pace"), QStringLiteral("format")}
        : QStringList{selector};
    for (const auto &name : selected) {
        QSaveFile output(outDir.filePath(name + QStringLiteral(".actual.json")));
        const QByteArray data("{\"error\":\"not ported\"}\n");
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size()
            || !output.commit())
            return fail(QStringLiteral("error: cannot write %1: %2")
                            .arg(output.fileName(), output.errorString()), 1);
    }
    return 0;
}
