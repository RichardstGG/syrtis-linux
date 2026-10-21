#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

class SingleInstanceTest final : public QObject {
    Q_OBJECT

public:
    QString helper;

private slots:
    void forwardsArgumentsAndWorkingDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkdir(QStringLiteral("primary")));
        QVERIFY(root.mkdir(QStringLiteral("secondary directory")));

        QProcess primary;
        const auto stopPrimary = qScopeGuard([&] {
            if (primary.state() != QProcess::NotRunning) {
                primary.kill();
                primary.waitForFinished(1000);
            }
        });
        primary.setWorkingDirectory(root.filePath(QStringLiteral("primary")));
        primary.start(helper, {});
        QVERIFY(primary.waitForStarted());
        QVERIFY2(primary.waitForReadyRead(5000), primary.readAllStandardError().constData());
        const auto ready = QJsonDocument::fromJson(primary.readLine()).object();
        QCOMPARE(ready.value("event").toString(), QStringLiteral("ready"));
        QCOMPARE(ready.value("arguments").toArray(), QJsonArray::fromStringList({helper}));
        QCOMPARE(ready.value("windows").toInt(), 1);

        const QList<QStringList> launches = {
            {},
            {QStringLiteral("--synthetic-option"), QStringLiteral("relative file.json"),
             QStringLiteral("合成參數"), QString(), QStringLiteral("--"), QStringLiteral("a b")},
            {QStringLiteral("--syrtis-singleinstance-forward")}
        };
        for (const auto &arguments : launches) {
            QProcess secondary;
            const auto stopSecondary = qScopeGuard([&] {
                if (secondary.state() != QProcess::NotRunning) {
                    secondary.kill();
                    secondary.waitForFinished(1000);
                }
            });
            const auto workingDirectory = root.filePath(QStringLiteral("secondary directory"));
            secondary.setWorkingDirectory(workingDirectory);
            secondary.start(helper, arguments);
            QVERIFY(secondary.waitForStarted());
            QVERIFY2(secondary.waitForFinished(5000), secondary.readAllStandardError().constData());
            QCOMPARE(secondary.exitStatus(), QProcess::NormalExit);
            QCOMPARE(secondary.exitCode(), 0);
            // Only a process that passes the uniqueness gate can report/create a window.
            QVERIFY(secondary.readAllStandardOutput().isEmpty());

            if (!primary.canReadLine())
                QVERIFY2(primary.waitForReadyRead(5000), primary.readAllStandardError().constData());
            const auto activation = QJsonDocument::fromJson(primary.readLine()).object();
            QCOMPARE(activation.value("event").toString(), QStringLiteral("activation"));
            QStringList expected{helper};
            expected.append(arguments);
            QCOMPARE(activation.value("arguments").toArray(), QJsonArray::fromStringList(expected));
            QCOMPARE(activation.value("workingDirectory").toString(), workingDirectory);
            QCOMPARE(activation.value("windows").toInt(), 1);
            QCOMPARE(primary.state(), QProcess::Running);
            QVERIFY(!primary.canReadLine());
        }
        primary.terminate();
        QVERIFY(primary.waitForFinished(5000));
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2)
        return 1;
    SingleInstanceTest test;
    test.helper = app.arguments().at(1);
    return QTest::qExec(&test, 1, argv);
}

#include "SingleInstanceTest.moc"
