#include "SingleInstance.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

static void report(const QJsonObject &event)
{
    QTextStream(stdout) << QJsonDocument(event).toJson(QJsonDocument::Compact) << Qt::endl;
}

int main(int argc, char *argv[])
{
    Syrtis::SingleInstance::LaunchArguments launchArguments(argc, argv);
    QGuiApplication app(launchArguments.argc, launchArguments.argv.data());
    app.setApplicationName(QStringLiteral("syrtis-singleinstance-test"));
    app.setOrganizationDomain(QStringLiteral("example.org"));

    Syrtis::SingleInstance singleInstance(launchArguments);
    QWindow window;
    window.show();
    QObject::connect(&singleInstance, &Syrtis::SingleInstance::activationRequested,
                     &app, [](const QStringList &arguments, const QString &workingDirectory) {
        report({{QStringLiteral("event"), QStringLiteral("activation")},
                {QStringLiteral("arguments"), QJsonArray::fromStringList(arguments)},
                {QStringLiteral("workingDirectory"), workingDirectory},
                {QStringLiteral("windows"), QGuiApplication::allWindows().size()}});
    });
    report({{QStringLiteral("event"), QStringLiteral("ready")},
            {QStringLiteral("arguments"), QJsonArray::fromStringList(app.arguments())},
            {QStringLiteral("windows"), QGuiApplication::allWindows().size()}});
    QTimer::singleShot(20000, &app, &QCoreApplication::quit);
    return app.exec();
}
