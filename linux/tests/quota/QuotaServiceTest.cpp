#include "QuotaService.h"
#include "FakeFfi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>
#include <limits>
#include <memory>

using namespace Syrtis;

namespace {
const QString stamp = QStringLiteral("2026-01-01T00:00:00.000Z");

QJsonObject agent(double remaining = 72, std::optional<QString> account = std::nullopt,
                  std::optional<QString> error = std::nullopt, bool windows = true)
{
    QJsonObject result{{"clientId", "codex"}, {"source", "synthetic"}, {"updatedAt", stamp},
        {"windows", windows ? QJsonArray{QJsonObject{{"cardId", "session.v1"},
            {"label", "Session"}, {"usedPercent", 100 - remaining},
            {"remainingPercent", remaining}}} : QJsonArray{}}};
    if (account) result.insert("accountKey", *account);
    if (error) result.insert("error", *error);
    return result;
}

QByteArray reply(QJsonArray agents = {agent()}, qint64 generation = 1)
{
    return QJsonDocument(QJsonObject{{"ok", true}, {"data", QJsonObject{
        {"generatedAt", stamp}, {"publicationGeneration", generation}, {"agents", agents}}}})
        .toJson(QJsonDocument::Compact);
}
} // namespace

class QuotaServiceTest final : public QObject {
    Q_OBJECT
    QDateTime now_;
    QuotaService::Clock clock() { return [this] { return now_; }; }
    void advance() { now_ = now_.addSecs(51); }

private slots:
    void init()
    {
        FakeFfi::reset();
        now_ = QDateTime::fromString(stamp, Qt::ISODateWithMs);
    }

    void cleanup()
    {
        const auto stats = FakeFfi::stats();
        QCOMPARE(stats.allocated, stats.freed);
        QCOMPARE(stats.invalidFrees, 0);
        QVERIFY(!stats.freedOnMainThread);
        QVERIFY(!stats.gateTimedOut);
        for (const auto &call : stats.calls) {
            QCOMPARE(call.name, "tb_agent_usage");
            QVERIFY(!call.onMainThread);
        }
    }

    void selectionPolicy()
    {
        const auto object = QJsonDocument::fromJson(reply({agent(72), agent(19, "account-b")})).object();
        std::optional<syrtis::core::AgentUsagePayload> payload = syrtis::core::AgentUsagePayload::decode({object["data"], {}});
        QCOMPARE(QuotaSelectionPolicy::effectiveSelection(payload, "codex|Session"), "codex|session.v1");
        QCOMPARE(QuotaSelectionPolicy::migrationToPersist(payload, "codex|Session").value(), "codex|session.v1");
        QVERIFY(!QuotaSelectionPolicy::migrationToPersist(payload, "auto"));
        QVERIFY(!QuotaSelectionPolicy::migrationToPersist(payload, "codex|session.v1"));
        QCOMPARE(QuotaSelectionPolicy::resolve(payload, "auto")->accountKey.value(), "account-b");
        QVERIFY(!QuotaSelectionPolicy::resolve(payload, "auto", {"codex"}));
        // Explicit choices ignore Auto's exclusions and use the exact account.
        QCOMPARE(QuotaSelectionPolicy::resolve(payload, "codex|Session", {"codex"}, "account-b")
            ->window.remainingPercent, 19.0);
        QCOMPARE(QuotaSelectionPolicy::resolvedAt(*payload, "codex|Session").value(), now_);
        QCOMPARE(QuotaSelectionPolicy::resolveRemainingPercent(std::nullopt, "auto", {}, 44).value(), 44.0);
        QVERIFY(!QuotaSelectionPolicy::resolveRemainingPercent(payload, "codex|missing", {}, 44));
        payload->agents[0].windows[0].remainingPercent = std::numeric_limits<double>::infinity();
        QVERIFY(!QuotaSelectionPolicy::resolveRemainingPercent(payload, "codex|Session", {}, 44));
        payload->agents[0].windows[0].remainingPercent = 72;
        auto duplicate = payload->agents[0].windows[0];
        duplicate.cardId = "other.v1";
        payload->agents[0].windows.append(duplicate);
        QVERIFY(!QuotaSelectionPolicy::migrationToPersist(payload, "codex|Session"));
    }

    void lastGoodAndRecovery()
    {
        FakeFfi::setReply(reply());
        QuotaService service(nullptr, clock());
        QVERIFY(!service.attempted());
        QVERIFY(!service.remainingPercent().isValid());
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.attempted());
        QCOMPARE(service.remainingPercent().toDouble(), 72.0);
        QVERIFY(!service.stale());
        const auto fetched = service.resolvedAt();
        advance();
        FakeFfi::setReply(R"({"ok":false,"err":"synthetic_failure"})");
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 72.0);
        QCOMPARE(service.resolvedAt(), fetched);
        QCOMPARE(service.payload()->publicationGeneration.value(), quint64(1));
        QVERIFY(service.stale());
        QCOMPARE(service.error(), "synthetic_failure");
        FakeFfi::setReply(reply({agent(61)}, 2));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 61.0);
        QVERIFY(!service.stale());
        QVERIFY(service.error().isEmpty());
    }

    void failure_data()
    {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("outer-error") << QByteArray(R"({"ok":false,"err":"synthetic_failure"})");
        QTest::newRow("empty-outer-error") << QByteArray(R"({"ok":false,"err":""})");
        QTest::newRow("null-pointer") << QByteArray();
        QTest::newRow("invalid-json") << QByteArray("{");
        QTest::newRow("invalid-envelope") << QByteArray(R"({"ok":true})");
        QTest::newRow("decode-failure") << QByteArray(R"({"ok":true,"data":{"agents":[]}})");
        QTest::newRow("null-data") << QByteArray(R"({"ok":true,"data":null})");
        QTest::newRow("invalid-window") << reply({agent(101)});
    }

    void failure()
    {
        QFETCH(QByteArray, response);
        QuotaService service(nullptr, clock());
        FakeFfi::setReply(response);
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.attempted());
        QVERIFY(!service.remainingPercent().isValid());
        QVERIFY(!service.stale()); // No reading to misrepresent as zero/fresh.
        QVERIFY(!service.error().isEmpty());
        FakeFfi::setReply(reply());
        service.refresh();
        QTRY_VERIFY(!service.busy());
        advance();
        FakeFfi::setReply(response);
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 72.0);
        QVERIFY(service.stale());
    }

    void successfulTerminalClearsLastGood()
    {
        QuotaService service(nullptr, clock());
        service.setSelection("codex|session.v1");
        FakeFfi::setReply(reply());
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.reading());
        advance();
        FakeFfi::setReply(reply({agent(72, std::nullopt, "synthetic_logout", false)}, 2));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.reading());
        QVERIFY(!service.resolvedAt());
        QCOMPARE(service.error(), "synthetic_logout");
        advance();
        FakeFfi::setReply(R"({"ok":false,"err":"synthetic_failure"})");
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.remainingPercent().isValid());
    }

    void providerFallbackAndSnapshotAge()
    {
        QuotaService service(nullptr, clock());
        service.setSelection("codex|Session", "account-a");
        FakeFfi::setReply(reply({agent(36, "account-a", "synthetic_transient")}));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 36.0);
        QCOMPARE(service.resolvedAt().value(), now_);
        QCOMPARE(service.error(), "synthetic_transient");
        QVERIFY(service.stale());
        service.setSelection("auto");
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.reading()); // Auto never chooses an errored snapshot.
        service.setSelection("codex|session.v1", "account-a");
        FakeFfi::setReply(reply({agent(36, "account-a")}, 2));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        now_ = now_.addSecs(30 * 60);
        QVERIFY(!service.stale());
        now_ = now_.addMSecs(1);
        QVERIFY(service.stale()); // Strictly older than 30 minutes, as Swift.
        advance();
        FakeFfi::setReply(reply({agent(35, "account-a")}, 3));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.stale()); // A new publication cannot freshen an old stamp.
    }

    void accountSwitchNeverRevivesLastGood()
    {
        QuotaService service(nullptr, clock());
        service.setSelection("codex|session.v1", "account-a");
        FakeFfi::setReply(reply({agent(72, "account-a"), agent(19, "account-b")}));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 72.0);
        service.setSelection("codex|session.v1", "account-b");
        QVERIFY(!service.remainingPercent().isValid());
        QVERIFY(!service.payload());
        FakeFfi::setReply(R"({"ok":false,"err":"synthetic_failure"})");
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.reading());
        FakeFfi::setReply(reply({agent(72, "account-a"), agent(19, "account-b")}, 2));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 19.0);
        QCOMPARE(service.reading()->accountKey.value(), "account-b");
        service.setSelection("codex|session.v1", "account-a");
        FakeFfi::setReply(R"({"ok":false,"err":"synthetic_failure"})");
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.reading());
        service.setSelection("codex|session.v1");
        FakeFfi::setReply(reply({agent()}, 3));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(service.reading());
        service.invalidateAccounts(); // Primary binding may change without a key change.
        QVERIFY(!service.reading());
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QVERIFY(!service.reading()->accountKey);
    }

    void accountChangeDuringFetch()
    {
        QuotaService service(nullptr, clock());
        service.setSelection("codex|session.v1", "account-a");
        FakeFfi::setReply(reply({agent(72, "account-a")}));
        FakeFfi::block("tb_agent_usage");
        service.refresh();
        QTRY_COMPARE(FakeFfi::entered.available(), 1);
        service.setSelection("codex|session.v1", "account-b");
        FakeFfi::setReply(reply({agent(19, "account-b")}, 2));
        FakeFfi::block({});
        service.refresh();
        FakeFfi::released.release();
        QSignalSpy changes(&service, &QuotaService::stateChanged);
        QTRY_VERIFY(!service.busy());
        QCOMPARE(changes.size(), 1); // Old A completion never publishes any state.
        QCOMPARE(service.remainingPercent().toDouble(), 19.0);
        QCOMPARE(service.payload()->agents[0].accountKey.value(), "account-b");
        QCOMPARE(FakeFfi::stats().calls.size(), 2);
    }

    void throttleAndPollingLifecycle()
    {
        QuotaService service(nullptr, clock());
        FakeFfi::setReply(reply());
        service.start(1);
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(FakeFfi::stats().calls.size(), 1);
        QTest::qWait(15);
        QCOMPARE(FakeFfi::stats().calls.size(), 1);
        advance();
        QTRY_COMPARE(FakeFfi::stats().calls.size(), 2);
        QTRY_VERIFY(!service.busy());
        service.stop();
        advance();
        QTest::qWait(15);
        QCOMPARE(FakeFfi::stats().calls.size(), 2);
    }

    void publicationOrdering()
    {
        QuotaService service(nullptr, clock());
        FakeFfi::setReply(reply({agent(19)}, 9));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        advance();
        FakeFfi::setReply(reply({agent(72)}, 8));
        service.refresh();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.remainingPercent().toDouble(), 19.0);
        QCOMPARE(service.payload()->publicationGeneration.value(), quint64(9));
    }

    void destructionDoesNotWaitForFfi()
    {
        FakeFfi::setReply(reply());
        FakeFfi::block("tb_agent_usage");
        auto service = std::make_unique<QuotaService>();
        service->refresh();
        QTRY_COMPARE(FakeFfi::entered.available(), 1);
        service.reset(); // Must return while the uncancellable FFI is blocked.
        FakeFfi::released.release();
        QTRY_COMPARE(FakeFfi::stats().freed, 1);
    }
};

QTEST_GUILESS_MAIN(QuotaServiceTest)
#include "QuotaServiceTest.moc"
