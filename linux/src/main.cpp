#include "bridge/Bridge.h"
#include "quota/QuotaService.h"
#include "singleinstance/SingleInstance.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QWindow>

#include <cstdlib>

int main(int argc, char *argv[])
{
    Syrtis::SingleInstance::LaunchArguments arguments(argc, argv);
    QGuiApplication app(arguments.argc, arguments.argv.data());
    QCoreApplication::setApplicationName(QStringLiteral("Syrtis"));
    Syrtis::SingleInstance singleInstance(arguments);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption quitAfterStart(QStringLiteral("quit-after-start"),
                                          QStringLiteral("Exit after creating the window."));
    parser.addOption(quitAfterStart);
    const QCommandLineOption probe(QStringLiteral("probe"),
                                   QStringLiteral("Probe local sessions asynchronously and exit."));
    parser.addOption(probe);
    parser.process(app);

    Syrtis::Bridge bridge;
    if (parser.isSet(probe)) {
        QObject::connect(&bridge, &Syrtis::Bridge::completed, &app,
                         [&app](quint64, const QJsonObject &envelope) {
            app.exit(envelope.value("ok").toBool() ? EXIT_SUCCESS : EXIT_FAILURE);
        });
        bridge.probe();
        return app.exec();
    }

    Syrtis::QuotaService quotaService;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("quotaService"), QVariant::fromValue(&quotaService)}});
    QObject::connect(&singleInstance, &Syrtis::SingleInstance::activationRequested,
                     &engine, [&engine] {
        for (auto *root : engine.rootObjects()) {
            if (auto *window = qobject_cast<QWindow *>(root)) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });
    engine.loadFromModule("Syrtis", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    if (parser.isSet(quitAfterStart))
        QTimer::singleShot(0, &app, &QCoreApplication::quit);

    return app.exec();
}
