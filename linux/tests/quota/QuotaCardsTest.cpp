#include "QuotaService.h"
#include "FakeFfi.h"
#include "UsagePace.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

using namespace Syrtis;

namespace {
const QString stamp = QStringLiteral("2026-01-01T00:00:00.000Z");
const QString reset = QStringLiteral("2026-01-01T03:00:00.000Z");
const QStringList clients{"claude", "codex", "antigravity"};

QJsonObject agent(const QString &client, const QString &source = "synthetic",
                  const QString &error = {}, bool windows = true, const QString &account = {})
{
    QJsonObject result{{"clientId", client}, {"source", source}, {"updatedAt", stamp},
        {"windows", windows ? QJsonArray{
            QJsonObject{{"cardId", "five-hour.v1"}, {"label", "5H"},
                {"usedPercent", 27.5}, {"remainingPercent", 72.5}, {"resetsAt", reset}},
            QJsonObject{{"cardId", "weekly.v1"}, {"label", "Weekly"},
                {"usedPercent", 81}, {"remainingPercent", 19},
                {"resetsAt", "2026-01-04T00:00:00.000Z"}}} : QJsonArray{}}};
    if (!error.isEmpty()) result.insert("error", error);
    if (!account.isEmpty()) result.insert("accountKey", account);
    return result;
}

QByteArray reply(const QJsonArray &agents, int generation = 1)
{
    return QJsonDocument(QJsonObject{{"ok", true}, {"data", QJsonObject{
        {"generatedAt", stamp}, {"publicationGeneration", generation}, {"agents", agents}}}})
        .toJson(QJsonDocument::Compact);
}

// Repeater delegates belong to the visual tree, which need not share QObject
// ownership with their visual parent.
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = item(child, name)) return found;
    return nullptr;
}
}

class QuotaCardsTest final : public QObject {
    Q_OBJECT
    QDateTime now_;
    std::unique_ptr<QuotaService> service_;
    std::unique_ptr<QQmlEngine> engine_;
    std::unique_ptr<QQuickWindow> window_;
    QQuickItem *view_ = nullptr;
    QList<QQmlError> warnings_;

    QQuickItem *card(const QString &client, const QString &account = {})
    {
        return item(view_, "quotaCard-" + syrtis::core::AccountIdentity{client,
            account.isEmpty() ? std::nullopt : std::optional<QString>(account)}.windowKey("limits"));
    }
    QVariantMap data(QQuickItem *cardItem) { return cardItem->property("card").toMap(); }
    void fetch(const QByteArray &response)
    {
        FakeFfi::setReply(response);
        service_->refresh();
        QTRY_VERIFY(!service_->busy());
        QCoreApplication::processEvents();
    }

private slots:
    void init()
    {
        FakeFfi::reset();
        warnings_.clear();
        now_ = QDateTime::fromString(stamp, Qt::ISODateWithMs);
        service_ = std::make_unique<QuotaService>(nullptr, [this] { return now_; });
        engine_ = std::make_unique<QQmlEngine>();
        connect(engine_.get(), &QQmlEngine::warnings, this,
                [this](const QList<QQmlError> &warnings) { warnings_.append(warnings); });
        QQmlComponent component(engine_.get(), QUrl::fromLocalFile(
            QStringLiteral(QUOTA_QML_DIR "/QuotaView.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        view_ = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
            {{"service", QVariant::fromValue(service_.get())}}));
        QVERIFY2(view_, qPrintable(component.errorString()));
        window_ = std::make_unique<QQuickWindow>();
        window_->resize(420, 720);
        view_->setParentItem(window_->contentItem());
        view_->setSize(QSizeF(420, 720));
        window_->show();
    }

    void cleanup()
    {
        delete view_;
        view_ = nullptr;
        window_.reset();
        engine_.reset();
        service_.reset();
        QVERIFY2(warnings_.isEmpty(), warnings_.isEmpty() ? "" : qPrintable(warnings_.first().toString()));
        const auto stats = FakeFfi::stats();
        QCOMPARE(stats.allocated, stats.freed);
        QCOMPARE(stats.invalidFrees, 0);
        QVERIFY(!stats.freedOnMainThread);
        for (const auto &call : stats.calls) QVERIFY(!call.onMainThread);
    }

    void states_data()
    {
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("error");
        QTest::addColumn<bool>("windows");
        QTest::newRow("fresh") << "fresh" << "synthetic" << "" << true;
        QTest::newRow("provider-last-good") << "stale" << "synthetic" << "synthetic_transient" << true;
        QTest::newRow("unsupported") << "unsupported" << "unsupported" << "" << false;
        QTest::newRow("unsupported-discards-windows") << "unsupported" << "unsupported" << "" << true;
        QTest::newRow("needs-login") << "needs-login" << "unconfigured" << "synthetic_login" << false;
        QTest::newRow("error") << "error" << "synthetic" << "synthetic_terminal" << false;
        QTest::newRow("no-windows") << "unsupported" << "synthetic" << "" << false;
        QTest::newRow("setup-discards-windows") << "needs-login" << "unconfigured" << "" << true;
    }

    void states()
    {
        QFETCH(QString, state);
        QFETCH(QString, source);
        QFETCH(QString, error);
        QFETCH(bool, windows);
        QJsonArray agents;
        for (const auto &client : clients) agents.append(agent(client, source, error, windows));
        fetch(reply(agents));
        for (const auto &client : clients) {
            auto *shown = card(client);
            QVERIFY(shown);
            QCOMPARE(data(shown)["state"].toString(), state);
            QCOMPARE(item(shown, "quotaStatus")->property("text").toString(), state);
            const bool hasWindows = state == "fresh" || state == "stale";
            QCOMPARE(data(shown)["windows"].toList().size(), hasWindows ? 2 : 0);
            QCOMPARE(item(shown, "quotaGauge") != nullptr, hasWindows);
            if (hasWindows) {
                QCOMPARE(item(shown, "quotaRemaining")->property("text").toString(), "73% left");
                QCOMPARE(item(shown, "quotaGauge")->property("value").toDouble(), 72.5);
                QCOMPARE(item(shown, "quotaReset")->property("text").toString(),
                    syrtis::core::UsagePace::resetText(reset, now_.toMSecsSinceEpoch() / 1000.0 - 978307200.0).value());
                QCOMPARE(data(shown)["windows"].toList()[1].toMap()["label"].toString(), "Weekly");
            }
        }
        if (state == "needs-login") {
            const auto message = item(card("claude"), "quotaMessage")->property("text").toString();
            QVERIFY(message.contains("`claude`"));
            QVERIFY(message.contains("claude setup-token"));
            QVERIFY(message.contains("secret-tool store"));
            QVERIFY(!message.contains("Keychain"));
        }
        QCOMPARE(service_->summary().isEmpty(), state != "fresh");
        QTest::qWait(20);
        const auto image = window_->grabWindow();
        QVERIFY(!image.isNull());
        const auto screenshotDir = qEnvironmentVariable("SYRTIS_TEST_SCREENSHOTS");
        if (!screenshotDir.isEmpty())
            QVERIFY(image.save(screenshotDir + "/" + QString::fromLatin1(QTest::currentDataTag()) + ".png"));
    }

    void outerFailureRecoveryAndAge()
    {
        for (const auto &client : clients) {
            QVERIFY(card(client));
            QCOMPARE(data(card(client))["state"].toString(), "loading");
            QVERIFY(!item(card(client), "quotaGauge"));
        }
        fetch(R"({"ok":false,"err":"synthetic_failure"})");
        for (const auto &client : clients) {
            QCOMPARE(data(card(client))["state"].toString(), "error");
            QVERIFY(!item(card(client), "quotaGauge"));
        }
        QJsonArray agents;
        for (const auto &client : clients) agents.append(agent(client));
        fetch(reply(agents));
        const auto original = service_->cards()[0].toMap()["windows"];
        now_ = now_.addSecs(51);
        fetch(R"({"ok":false,"err":"synthetic_failure"})");
        for (const auto &client : clients) QCOMPARE(data(card(client))["state"].toString(), "stale");
        QCOMPARE(service_->cards()[0].toMap()["windows"].toList()[0].toMap()["remainingText"],
            original.toList()[0].toMap()["remainingText"]);
        QVERIFY(service_->summary().isEmpty());
        fetch(reply(agents, 2));
        QCOMPARE(data(card("codex"))["state"].toString(), "fresh");
        QCOMPARE(service_->summary()["remainingText"].toString(), "19% left");
        QCOMPARE(service_->summary()["label"].toString(), "Weekly");
        now_ = now_.addSecs(30 * 60);
        fetch(reply(agents, 3));
        for (const auto &client : clients) QCOMPARE(data(card(client))["state"].toString(), "stale");
        QVERIFY(service_->summary().isEmpty());
    }

    void terminalAndAccountIsolation()
    {
        service_->setSelection("codex|five-hour.v1", "account-a");
        fetch(reply({agent("codex", "synthetic", {}, true, "account-a"),
                     agent("codex", "synthetic", {}, true, "account-b")}));
        QVERIFY(card("codex", "account-a"));
        QVERIFY(!card("codex", "account-b"));
        service_->setSelection("codex|five-hour.v1", "account-b");
        fetch(R"({"ok":false,"err":"synthetic_failure"})");
        QVERIFY(!card("codex", "account-a"));
        QVERIFY(card("codex", "account-b"));
        QVERIFY(!item(card("codex", "account-b"), "quotaGauge"));
        fetch(reply({agent("codex", "unconfigured", {}, false, "account-b")}, 2));
        QCOMPARE(data(card("codex", "account-b"))["state"].toString(), "needs-login");
        QVERIFY(!item(card("codex", "account-b"), "quotaGauge"));
        service_->invalidateAccounts();
        fetch(reply({agent("codex", "synthetic", {}, true, "account-a")}, 3));
        QCOMPARE(data(card("codex", "account-b"))["state"].toString(), "unsupported");
        QVERIFY(!item(card("codex", "account-b"), "quotaGauge"));
    }

    void mixedStatesAndProviderMetadata()
    {
        auto codex = agent("codex");
        auto windows = codex["windows"].toArray();
        auto window = windows[0].toObject();
        window.remove("resetsAt");
        window.insert("resetText", "Synthetic provider reset metadata");
        window.insert("remainingPercent", 0);
        window.insert("usedPercent", 100);
        windows[0] = window;
        codex.insert("windows", windows);
        fetch(reply({agent("claude", "unconfigured", {}, false), codex,
            agent("antigravity", "synthetic", "synthetic_transient")}));
        QCOMPARE(data(card("claude"))["state"].toString(), "needs-login");
        QCOMPARE(data(card("codex"))["state"].toString(), "fresh");
        QCOMPARE(data(card("antigravity"))["state"].toString(), "stale");
        QCOMPARE(item(card("codex"), "quotaRemaining")->property("text").toString(), "0% left");
        QCOMPARE(item(card("codex"), "quotaGauge")->property("value").toDouble(), 0.0);
        QCOMPARE(item(card("codex"), "quotaReset")->property("text").toString(),
            "Synthetic provider reset metadata");
        QCOMPARE(service_->summary()["name"].toString(), "Codex");
        QCOMPARE(service_->summary()["remainingText"].toString(), "0% left");
        now_ = now_.addSecs(51);
        fetch(reply({}, 2));
        for (const auto &client : clients) {
            QCOMPARE(data(card(client))["state"].toString(), "unsupported");
            QVERIFY(!item(card(client), "quotaGauge"));
        }
        now_ = now_.addSecs(51);
        fetch(R"({"ok":false,"err":"synthetic_failure"})");
        for (const auto &client : clients) QVERIFY(!item(card(client), "quotaGauge"));
    }

    void mainWindowPollingAndRefresh()
    {
        FakeFfi::setReply(reply({agent("claude"), agent("codex"), agent("antigravity")}));
        QQmlApplicationEngine appEngine;
        connect(&appEngine, &QQmlEngine::warnings, this,
            [this](const QList<QQmlError> &warnings) { warnings_.append(warnings); });
        appEngine.setInitialProperties({{"quotaService", QVariant::fromValue(service_.get())}});
        appEngine.load(QUrl::fromLocalFile(QStringLiteral(QUOTA_QML_DIR "/Main.qml")));
        QCOMPARE(appEngine.rootObjects().size(), 1);
        auto *main = qobject_cast<QQuickWindow *>(appEngine.rootObjects().front());
        QVERIFY(main);
        QTRY_VERIFY(service_->attempted() && !service_->busy());
        QCOMPARE(FakeFfi::stats().calls.size(), 1);
        auto *refresh = item(main->contentItem(), "quotaRefresh");
        QVERIFY(refresh);
        now_ = now_.addSecs(51);
        QVERIFY(QMetaObject::invokeMethod(refresh, "clicked"));
        QTRY_VERIFY(!service_->busy());
        QCOMPARE(FakeFfi::stats().calls.size(), 2);
        main->hide();
        now_ = now_.addSecs(51);
        QTest::qWait(20);
        QCOMPARE(FakeFfi::stats().calls.size(), 2);
        main->show();
        QTRY_VERIFY(!service_->busy());
        QCOMPARE(FakeFfi::stats().calls.size(), 3);
        service_->stop();
    }
};

QTEST_MAIN(QuotaCardsTest)
#include "QuotaCardsTest.moc"
