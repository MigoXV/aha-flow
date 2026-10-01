#include "core/engine_config.h"

#include <QTest>
#include <QUrlQuery>

class EngineConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsTranscriptionSession()
    {
        aha::EngineConfig config;
        const QUrl url = config.realtimeUrl();
        QCOMPARE(url.scheme(), QStringLiteral("wss"));
        QCOMPARE(url.path(), QStringLiteral("/v1/realtime"));
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QStringLiteral("intent")), QStringLiteral("transcription"));
        QCOMPARE(query.queryItemValue(QStringLiteral("x_aha")), QStringLiteral("v1"));
        QVERIFY(!query.hasQueryItem(QStringLiteral("model")));
    }

    void stripsPreviousPathAndQuery()
    {
        aha::EngineConfig config;
        config.baseUrl = QStringLiteral(" http://localhost:10000/old?token=old#fragment ");
        QCOMPARE(config.modelsUrl(), QUrl(QStringLiteral("http://localhost:10000/v1/models")));
        QCOMPARE(config.realtimeUrl().scheme(), QStringLiteral("ws"));
    }

    void canDisableExtension()
    {
        aha::EngineConfig config;
        config.enableAha = false;
        QVERIFY(!QUrlQuery(config.realtimeUrl()).hasQueryItem(QStringLiteral("x_aha")));
        config.enableAha = true;
        config.baseUrl = QStringLiteral("https://api.openai.com");
        QVERIFY(!QUrlQuery(config.realtimeUrl()).hasQueryItem(QStringLiteral("x_aha")));
    }

    void rejectsInvalidEndpoints_data()
    {
        QTest::addColumn<QString>("address");
        QTest::newRow("empty") << QString();
        QTest::newRow("no-scheme") << QStringLiteral("localhost:10000");
        QTest::newRow("file") << QStringLiteral("file:///tmp/socket");
        QTest::newRow("no-host") << QStringLiteral("https://");
        QTest::newRow("userinfo") << QStringLiteral("https://user:password@localhost");
    }

    void rejectsInvalidEndpoints()
    {
        QFETCH(QString, address);
        aha::EngineConfig config;
        config.baseUrl = address;
        QVERIFY(config.modelsUrl().isEmpty());
        QVERIFY(config.realtimeUrl().isEmpty());
    }
};

QTEST_GUILESS_MAIN(EngineConfigTest)
#include "engine_config_test.moc"
