#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Syrtis"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption quitAfterStart(QStringLiteral("quit-after-start"),
                                          QStringLiteral("Exit after creating the window."));
    parser.addOption(quitAfterStart);
    parser.process(app);

    QQmlApplicationEngine engine;
    engine.loadFromModule("Syrtis", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    if (parser.isSet(quitAfterStart))
        QTimer::singleShot(0, &app, &QCoreApplication::quit);

    return app.exec();
}
