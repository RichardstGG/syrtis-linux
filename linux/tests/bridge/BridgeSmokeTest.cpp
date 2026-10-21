#include "Bridge.h"

#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include <atomic>

namespace {
std::atomic<bool> probedOffMainThread = false;
}

extern "C" char *__real_tb_probe();
extern "C" char *__wrap_tb_probe()
{
    probedOffMainThread = QThread::currentThread() != QCoreApplication::instance()->thread();
    return __real_tb_probe();
}

class BridgeSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void realProbe()
    {
        Syrtis::Bridge bridge;
        QSignalSpy spy(&bridge, &Syrtis::Bridge::completed);
        bool completedOnMainThread = false;
        connect(&bridge, &Syrtis::Bridge::completed, this, [&] {
            completedOnMainThread = QThread::currentThread() == thread();
        });

        const auto id = bridge.probe();
        QCOMPARE(spy.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QCOMPARE(spy[0][0].toULongLong(), id);
        const auto envelope = qvariant_cast<QJsonObject>(spy[0][1]);
        QCOMPARE(envelope.value("ok"), QJsonValue(true));
        QCOMPARE(envelope.value("messages"), QJsonValue(1));
        QVERIFY(!envelope.contains("data")); // tb_probe keeps its legacy shape.
        QVERIFY(probedOffMainThread.load());
        QVERIFY(completedOnMainThread);
    }
};

QTEST_GUILESS_MAIN(BridgeSmokeTest)
#include "BridgeSmokeTest.moc"
