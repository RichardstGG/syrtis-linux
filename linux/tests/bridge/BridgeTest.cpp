#include "Bridge.h"
#include "FakeFfi.h"

#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <limits>
#include <memory>

using Syrtis::Bridge;

class BridgeTest final : public QObject {
    Q_OBJECT

private slots:
    void init() { FakeFfi::reset(); }

    void cleanup()
    {
        const auto state = FakeFfi::stats();
        QCOMPARE(state.allocated, state.freed);
        QCOMPARE(state.invalidFrees, 0);
        QVERIFY(!state.freedOnMainThread);
        QVERIFY(!state.gateTimedOut);
        for (const auto &call : state.calls)
            QVERIFY(!call.onMainThread);
    }

    void envelopes_data()
    {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<QByteArray>("expected");
        const auto pass = [](const char *name, QByteArray json) {
            QTest::newRow(name) << json << json;
        };
        pass("object", R"({"ok":true,"data":{"tokens":150,"label":"合成"},"additive":[1,true]})");
        pass("array", R"({"ok":true,"data":[{"tokens_per_min":1.5}]})");
        pass("null-data", R"({"ok":true,"data":null})");
        pass("scalar-data", R"({"ok":true,"data":0})");
        pass("error", R"({"ok":false,"err":"synthetic_failure","detail":{"retry":false}})");
        QTest::newRow("null-pointer") << QByteArray()
            << QByteArray(R"({"ok":false,"err":"bridge_null_response"})");
        for (const auto &invalid : {QByteArray(""), QByteArray("not json"), QByteArray("{")})
            QTest::newRow(qPrintable("invalid-json-" + QString::fromUtf8(invalid))) << invalid
                << QByteArray(R"({"ok":false,"err":"bridge_invalid_json"})");
        for (const auto &invalid : {"[]", "{}", R"({"ok":1,"data":{}})",
                 R"({"ok":true})", R"({"ok":false})", R"({"ok":false,"err":1})"})
            QTest::newRow(invalid) << QByteArray(invalid)
                << QByteArray(R"({"ok":false,"err":"bridge_invalid_envelope"})");
    }

    void envelopes()
    {
        QFETCH(QByteArray, reply);
        QFETCH(QByteArray, expected);
        FakeFfi::setReply(reply);
        Bridge bridge;
        QSignalSpy spy(&bridge, &Bridge::completed);
        const auto id = bridge.graph();
        QCOMPARE(spy.size(), 0); // Completion is always asynchronous.
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(spy[0][0].toULongLong(), id);
        QCOMPARE(qvariant_cast<QJsonObject>(spy[0][1]), QJsonDocument::fromJson(expected).object());
        QCOMPARE(FakeFfi::stats().freed, reply.isNull() ? 0 : 1);
    }

    void surfaceAndArguments()
    {
        Bridge bridge;
        QSignalSpy spy(&bridge, &Bridge::completed);
        const QList<quint64> ids = {
            bridge.probe(), bridge.graph(), bridge.refreshGraph("2026"),
            bridge.modelReport(""), bridge.hourlyReport("2025", "claude,codex"),
            bridge.agentsReport({}, "claude"), bridge.filterParityProbe(),
            bridge.usageTrace(600), bridge.tokensPerMin(), bridge.quotaProviderIds(),
            bridge.clientIds(), bridge.agentUsage(),
            bridge.quotaCurve("codex", {}, "weekly", std::numeric_limits<std::uint64_t>::max()),
            bridge.windowUsage({}, -1234, 1800000000000LL), bridge.setExtraScanPaths("{}")
        };
        QTRY_COMPARE(spy.size(), ids.size());
        QSet<quint64> received;
        for (const auto &event : spy) {
            const auto id = event[0].toULongLong();
            QVERIFY(!received.contains(id));
            received.insert(id);
            const auto envelope = qvariant_cast<QJsonObject>(event[1]);
            QVERIFY(envelope.value("ok").toBool());
            if (id == ids.front()) {
                QCOMPARE(envelope.value("messages").toInt(), 1);
                QVERIFY(!envelope.contains("data"));
            } else {
                QCOMPARE(envelope.value("data").toObject().value("totalTokens").toInt(), 150);
                QCOMPARE(envelope.value("extra").toString(), "retained");
            }
        }
        for (auto id : ids)
            QVERIFY(received.contains(id));

        QMap<QByteArray, QList<QByteArray>> actual;
        for (const auto &call : FakeFfi::stats().calls)
            actual.insert(call.name, call.arguments);
        const QMap<QByteArray, QList<QByteArray>> expected = {
            {"tb_probe", {}}, {"tb_graph", {QByteArray()}}, {"tb_refresh_graph", {"2026"}},
            {"tb_model_report", {""}}, {"tb_hourly_report", {"2025", "claude,codex"}},
            {"tb_agents_report", {QByteArray(), "claude"}}, {"tb_filter_parity_probe", {}},
            {"tb_usage_trace", {"600"}}, {"tb_tokens_per_min", {}}, {"tb_quota_provider_ids", {}},
            {"tb_client_ids", {}}, {"tb_agent_usage", {}},
            {"tb_quota_curve", {"codex", QByteArray(), "weekly", "18446744073709551615"}},
            {"tb_window_usage", {QByteArray(), "-1234", "1800000000000"}},
            {"tb_set_extra_scan_paths", {"{}"}}
        };
        QCOMPARE(actual, expected);
        QVERIFY(actual["tb_graph"][0].isNull());
        QVERIFY(!actual["tb_model_report"][0].isNull());
        QCOMPARE(FakeFfi::stats().freed, ids.size());
    }

    void guiResponsive_data()
    {
        QTest::addColumn<bool>("blockQuota");
        QTest::newRow("slow-report") << false;
        QTest::newRow("slow-quota") << true;
    }

    void guiResponsive()
    {
        QFETCH(bool, blockQuota);
        FakeFfi::block(blockQuota ? "tb_agent_usage" : "tb_graph");
        Bridge bridge;
        QSignalSpy spy(&bridge, &Bridge::completed);
        bool completedOnMain = false;
        connect(&bridge, &Bridge::completed, this, [&] {
            completedOnMain = QThread::currentThread() == thread();
        });
        const auto slow = blockQuota ? bridge.agentUsage() : bridge.graph();
        QTRY_VERIFY(FakeFfi::entered.available() > 0);
        bool heartbeat = false;
        QTimer::singleShot(0, this, [&] { heartbeat = true; });
        const auto fast = blockQuota ? bridge.graph() : bridge.agentUsage();
        QTRY_VERIFY(heartbeat);
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(spy[0][0].toULongLong(), fast);
        FakeFfi::released.release();
        QTRY_COMPARE(spy.size(), 2);
        QCOMPARE(spy[1][0].toULongLong(), slow);
        QVERIFY(completedOnMain);
    }

    void destructionAndInputLifetime()
    {
        FakeFfi::block("tb_graph");
        auto bridge = std::make_unique<Bridge>();
        bridge->graph();
        QTRY_VERIFY(FakeFfi::entered.available() > 0);
        {
            char storage[] = "2026";
            const auto year = QByteArray::fromRawData(storage, 4);
            bridge->refreshGraph(year); // Queued behind graph on the reports lane.
            std::fill_n(storage, 4, 'x');
        }
        bridge.reset(); // Must return while the FFI call is still blocked.
        QCOMPARE(FakeFfi::stats().freed, 0);
        FakeFfi::released.release();
        QTRY_COMPARE(FakeFfi::stats().freed, 2);
        const auto calls = FakeFfi::stats().calls;
        QCOMPARE(calls.size(), 2);
        QCOMPARE(calls[1].name, "tb_refresh_graph");
        QCOMPARE(calls[1].arguments, QList<QByteArray>{"2026"});
    }
};

QTEST_GUILESS_MAIN(BridgeTest)
#include "BridgeTest.moc"
