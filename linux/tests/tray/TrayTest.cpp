#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QSignalSpy>
#include <QWindow>
#include <QtTest>

class TrayTest final : public QObject {
    Q_OBJECT

private slots:
    void windowActions()
    {
        QQmlApplicationEngine engine;
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().front());
        QVERIFY(window);
        auto *tray = window->findChild<QObject *>("systemTray");
        QVERIFY(tray);
        auto *toggle = tray->findChild<QObject *>("trayToggleAction");
        auto *quit = tray->findChild<QObject *>("trayQuitAction");
        QVERIFY(toggle);
        QVERIFY(quit);
        QCOMPARE(tray->property("tooltip").toString(), QStringLiteral("Syrtis"));
        QVERIFY(QImageReader(QStringLiteral(":/src/tray/tray-icon.svg")).canRead());

        QVERIFY(window->isVisible());
        QCOMPARE(toggle->property("text").toString(), QStringLiteral("Hide window"));
        QVERIFY(QMetaObject::invokeMethod(toggle, "triggered"));
        QVERIFY(!window->isVisible());
        QCOMPARE(toggle->property("text").toString(), QStringLiteral("Show window"));
        QVERIFY(QMetaObject::invokeMethod(toggle, "triggered"));
        QVERIFY(window->isVisible());

        window->showMinimized();
        QCOMPARE(toggle->property("text").toString(), QStringLiteral("Show window"));
        QVERIFY(QMetaObject::invokeMethod(toggle, "triggered"));
        QCOMPARE(window->visibility(), QWindow::Windowed);

        const auto activate = [tray, toggle](const QString &reason) {
            QQmlExpression expression(qmlContext(toggle), tray,
                QStringLiteral("activated(Platform.SystemTrayIcon.%1)").arg(reason));
            expression.evaluate();
            if (expression.hasError())
                qWarning() << expression.error();
            return !expression.hasError();
        };
        QVERIFY(activate(QStringLiteral("Trigger")));
        QVERIFY(!window->isVisible());
        QVERIFY(activate(QStringLiteral("DoubleClick")));
        QVERIFY(window->isVisible());
        QVERIFY(activate(QStringLiteral("Context")));
        QVERIFY(window->isVisible());

        // Check the QML exit request without stopping the test application's loop.
        disconnect(&engine, &QQmlEngine::quit, nullptr, nullptr);
        QSignalSpy quitRequested(&engine, &QQmlEngine::quit);
        QVERIFY(QMetaObject::invokeMethod(quit, "triggered"));
        QCOMPARE(quitRequested.size(), 1);

        const bool trayAvailable = tray->property("available").toBool();
        QCOMPARE(window->close(), !trayAvailable);
        QVERIFY(!window->isVisible());
    }
};

QTEST_MAIN(TrayTest)
#include "TrayTest.moc"
