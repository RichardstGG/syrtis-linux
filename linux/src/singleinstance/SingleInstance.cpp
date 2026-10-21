#include "SingleInstance.h"

namespace {
char forwardingArgument[] = "--syrtis-singleinstance-forward";
}

namespace Syrtis {

SingleInstance::LaunchArguments::LaunchArguments(int count, char **values)
    : argc(count + 1), argv(values, values + count)
{
    // KF6 6.13 uses Activate() without extra arguments, losing the sender's cwd.
    // Always append a transport-only argument to force CommandLine(), including
    // when the user's last argument happens to equal our marker.
    argv.append(forwardingArgument);
    argv.append(nullptr);
}

SingleInstance::SingleInstance(LaunchArguments &arguments, QObject *parent)
    : QObject(parent), m_service(KDBusService::Unique)
{
    // The primary must parse only the original arguments. On Linux, Qt reads
    // arguments() from the argc reference and argv passed to its constructor.
    arguments.argv[--arguments.argc] = nullptr;
    connect(&m_service, &KDBusService::activateRequested,
            this, [this](QStringList forwarded, const QString &workingDirectory) {
        if (forwarded.size() > 1 && forwarded.last() == QLatin1String(forwardingArgument))
            forwarded.removeLast();
        emit activationRequested(forwarded, workingDirectory);
    });
}

} // namespace Syrtis
