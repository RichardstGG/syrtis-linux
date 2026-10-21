#pragma once

#include <KDBusService>
#include <QList>
#include <QObject>

namespace Syrtis {

class SingleInstance final : public QObject {
    Q_OBJECT

public:
    // Keep alive for the application lifetime; Qt may adjust argc/argv.
    struct LaunchArguments {
        LaunchArguments(int count, char **values);

        int argc;
        QList<char *> argv;
    };

    // Construct before loading QML or creating windows. A secondary exits here
    // after forwarding its arguments and working directory to the primary.
    explicit SingleInstance(LaunchArguments &arguments, QObject *parent = nullptr);

signals:
    void activationRequested(const QStringList &arguments, const QString &workingDirectory);

private:
    KDBusService m_service;
};

} // namespace Syrtis
