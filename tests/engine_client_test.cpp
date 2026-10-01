#include "network/engine_client.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class EngineClientTest final : public QObject {
    Q_OBJECT

private slots:
    void handlesHttpResponses_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("status");
        QTest::addColumn<bool>("success");
        QTest::newRow("models") << QByteArray(R"({"data":[{"id":"default-audio"},{"id":"default-audio"},{"id":null}]})") << 200 << true;
        QTest::newRow("empty-list") << QByteArray(R"({"data":[]})") << 200 << true;
        QTest::newRow("invalid-json") << QByteArray("not json") << 200 << false;
        QTest::newRow("invalid-schema") << QByteArray(R"({"models":[]})") << 200 << false;
        QTest::newRow("unauthorized") << QByteArray(R"({"error":"unauthorized"})") << 401 << false;
        QTest::newRow("redirect") << QByteArray() << 302 << false;
        QTest::newRow("oversized") << QByteArray(1024 * 1024 + 10, 'x') << 200 << false;
    }

    void handlesHttpResponses()
    {
        QFETCH(QByteArray, body);
        QFETCH(int, status);
        QFETCH(bool, success);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray received;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                received.append(socket->readAll());
                if (!received.contains("\r\n\r\n")) {
                    return;
                }
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });

        aha::EngineConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        config.apiKey = QStringLiteral("test-key");
        aha::EngineClient client;
        QSignalSpy ready(&client, &aha::EngineClient::modelsReady);
        QSignalSpy failed(&client, &aha::EngineClient::requestFailed);
        client.fetchModels(config);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size() + failed.size(), 1, 5000);
        QCOMPARE(ready.size(), success ? 1 : 0);
        QVERIFY(received.startsWith("GET /v1/models HTTP/1.1\r\n"));
        // HTTP 字段名不区分大小写；Qt 版本之间可能采用不同的序列化大小写。
        QVERIFY(received.toLower().contains("authorization: bearer test-key\r\n"));
        if (success) {
            const QStringList models = ready.at(0).at(0).toStringList();
            QCOMPARE(models.size(), body.contains("default-audio") ? 1 : 0);
        }
    }

    void rejectsInvalidAddress()
    {
        aha::EngineClient client;
        QSignalSpy failed(&client, &aha::EngineClient::requestFailed);
        aha::EngineConfig config;
        config.baseUrl = QStringLiteral("invalid");
        client.fetchModels(config);
        QCOMPARE(failed.size(), 1);
    }
};

QTEST_GUILESS_MAIN(EngineClientTest)
#include "engine_client_test.moc"
