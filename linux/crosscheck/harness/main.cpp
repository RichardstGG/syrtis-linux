#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QSaveFile>
#include <QtCore/QStringList>
#include <QtCore/QJsonDocument>
#include "QuotaTrendSelector.h"

#include <iostream>
#include <exception>
#include <stdexcept>

#include "UsagePace.h"
#include "FormatSelector.h"
#include "core-leaf.h"
#include "reports.h"
#include "agent-usage.h"

namespace {
int fail(const QString &message, int code)
{
    std::cerr << message.toStdString() << '\n';
    return code;
}

QString stageName(Syrtis::PaceStage stage)
{
    using Stage = Syrtis::PaceStage;
    switch (stage) {
    case Stage::onTrack: return QStringLiteral("onTrack");
    case Stage::slightlyAhead: return QStringLiteral("slightlyAhead");
    case Stage::ahead: return QStringLiteral("ahead");
    case Stage::farAhead: return QStringLiteral("farAhead");
    case Stage::slightlyBehind: return QStringLiteral("slightlyBehind");
    case Stage::behind: return QStringLiteral("behind");
    case Stage::farBehind: return QStringLiteral("farBehind");
    }
    throw std::logic_error("unknown pace stage");
}

template<typename T> QJsonValue jsonOptional(const std::optional<T> &value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonObject runUsagePace(const QDir &fixtures)
{
    QFile input(fixtures.filePath(QStringLiteral("usage-pace.json")));
    if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read usage-pace.json");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    const auto cases = document.object()["cases"];
    if (error.error != QJsonParseError::NoError || !document.isObject() || !cases.isArray())
        throw std::runtime_error("usage-pace.json must contain a cases array");
    for (const auto &item : cases.toArray())
        if (!item.isObject()) throw std::runtime_error("usage-pace.json must contain a cases array");

    QJsonObject output;
    qsizetype index = 0;
    for (const auto &item : cases.toArray()) {
        const auto c = item.toObject();
        const auto windowObject = c["window"];
        if (!c["name"].isString() || !c["kind"].isString()
            || (!windowObject.isObject() && !windowObject.isArray())) {
            output[QStringLiteral("invalid-case-%1").arg(index++)] = QJsonObject{{"error", "invalid case metadata"}};
            continue;
        }
        ++index;
        const auto name = c["name"].toString();
        const auto kind = c["kind"].toString();
        const auto window = Syrtis::UsageWindow::decode(windowObject);
        if (!window) {
            output[name] = QJsonObject{{"rejected", true}};
            continue;
        }
        if (kind == QStringLiteral("compute")) {
            const auto raw = c["mode"].isString() ? c["mode"].toString() : QStringLiteral("nil");
            std::optional<Syrtis::PaceMode> mode;
            if (raw == QStringLiteral("off")) mode = Syrtis::PaceMode::off;
            if (raw == QStringLiteral("linear")) mode = Syrtis::PaceMode::linear;
            if (raw == QStringLiteral("historical")) mode = Syrtis::PaceMode::historical;
            if (!mode) {
                output[name] = QJsonObject{{"error", QStringLiteral("unknown pace mode %1").arg(raw)}};
                continue;
            }
            const auto nowRaw = c["now"].isString() ? c["now"].toString() : QStringLiteral("nil");
            const auto now = Syrtis::parseRFC3339(nowRaw);
            if (!now) {
                output[name] = QJsonObject{{"error", QStringLiteral("unparseable now %1").arg(nowRaw)}};
                continue;
            }
            const auto pace = Syrtis::UsagePace::compute(*window, *mode, *now);
            if (!pace) {
                output[name] = QJsonValue::Null;
                continue;
            }
            output[name] = QJsonObject{
                {"stage", stageName(pace->stage)}, {"deltaPercent", pace->deltaPercent},
                {"expectedUsedPercent", pace->expectedUsedPercent}, {"actualUsedPercent", pace->actualUsedPercent},
                {"etaSeconds", jsonOptional(pace->etaSeconds)}, {"willLastToReset", pace->willLastToReset},
                {"label", pace->label()}, {"etaText", jsonOptional(pace->etaText())}};
        } else if (kind == QStringLiteral("runOutRisk")) {
            output[name] = jsonOptional(Syrtis::runOutRiskLabel(*window));
        } else {
            output[name] = QJsonObject{{"error", QStringLiteral("unknown kind %1").arg(kind)}};
        }
    }
    return output;
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
        return fail(QStringLiteral("usage: syrtis-crosscheck <fixtures-dir> <out-dir> [usage-pace|format|provider-quota-pace-v3|core-leaf|quota-trend|reports|agent-usage]"), 2);

    const QStringList knownSelectors{
        QStringLiteral("usage-pace"), QStringLiteral("format"),
        QStringLiteral("provider-quota-pace-v3"), QStringLiteral("core-leaf"),
        QStringLiteral("quota-trend"), QStringLiteral("reports"),
        QStringLiteral("agent-usage")};
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
        QByteArray data("{\"error\":\"not ported\"}\n");
        if (name == QStringLiteral("usage-pace") || name == QStringLiteral("format")) {
            try {
                data = QJsonDocument(name == QStringLiteral("format")
                    ? runFormat(QDir(args[0])) : runUsagePace(QDir(args[0]))).toJson();
            } catch (const std::exception &error) {
                return fail(QString::fromUtf8(error.what()), 1);
            }
        }
        if (name == QStringLiteral("quota-trend")) {
            try {
                data = QJsonDocument(quotaTrendSelector(QDir(args[0]))).toJson();
            } catch (const std::exception &error) {
                return fail(QString::fromUtf8(error.what()), 1);
            }
        }
        try {
            if (name == QStringLiteral("core-leaf")) data = coreLeaf(args[0]);
            if (name == QStringLiteral("reports")) data = reports(args[0]);
            if (name == QStringLiteral("agent-usage")) data = agentUsage(args[0]);
        } catch (const std::exception &error) {
            return fail(QString::fromUtf8(error.what()), 1);
        }
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size()
            || !output.commit())
            return fail(QStringLiteral("error: cannot write %1: %2")
                            .arg(output.fileName(), output.errorString()), 1);
    }
    return 0;
}
