#include "network/text_correction.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class CorrectionTest final : public QObject
{
    Q_OBJECT
  private slots:
    void streamSuccessAndFailure_data()
    {
        QTest::addColumn<QString>("mode");
        for (const char *mode : {"success", "truncated", "empty", "sequence", "disconnect", "http", "timeout"})
            QTest::newRow(mode) << QString::fromLatin1(mode);
    }
    void streamSuccessAndFailure()
    {
        QFETCH(QString, mode);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray request;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                request += socket->readAll();
                const auto split = request.indexOf("\r\n\r\n");
                if (split < 0 || !request.mid(split + 4).endsWith('}'))
                    return;
                if (mode == "timeout")
                    return;
                QByteArray body;
                const auto event = [&body](QJsonObject payload) {
                    body += "data: " + QJsonDocument(payload).toJson(QJsonDocument::Compact) + "\r\n\r\n";
                };
                event({{"type", "response.output_text.delta"}, {"delta", "纠正<KE"}, {"sequence_number", 1}});
                event({{"type", "response.output_text.delta"},
                       {"delta", "Y>[实体]"},
                       {"sequence_number", mode == "sequence" ? 1 : 2}});
                if (mode == "truncated")
                    event({{"type", "response.incomplete"}});
                else if (mode != "disconnect") {
                    const QJsonObject part{{"type", "output_text"}, {"text", mode == "empty" ? "" : "纠正<KEY>[实体]"}};
                    const QJsonObject item{{"type", "message"}, {"role", "assistant"}, {"content", QJsonArray{part}}};
                    event({{"type", "response.completed"},
                           {"response", QJsonObject{{"status", "completed"}, {"output", QJsonArray{item}}}}});
                }
                socket->write((mode == "http" ? "HTTP/1.1 500 Error\r\n" : "HTTP/1.1 200 OK\r\n") +
                              QByteArray("Content-Type: text/event-stream\r\nContent-Length: ") +
                              QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        aha::CorrectionConfig config;
        config.enabled = true;
        config.responsesUrl = QStringLiteral("http://127.0.0.1:%1/v1/responses").arg(server.serverPort());
        aha::TextCorrection correction(config, false, nullptr, mode == "timeout" ? 100 : 5000);
        QSignalSpy updates(&correction, &aha::TextCorrection::changed);
        const aha::Transcript original{"one", "原文", true, false, false, {}};
        correction.enqueue(original);
        QTRY_VERIFY_WITH_TIMEOUT(
            updates.size() >= 2 && !qvariant_cast<aha::Transcript>(updates.last().first()).correcting, 6000);
        const auto result = qvariant_cast<aha::Transcript>(updates.last().first());
        QCOMPARE(result.text, mode == "success" ? QStringLiteral("纠正") : QStringLiteral("原文"));
        QCOMPARE(result.correctionError.isEmpty(), mode == "success");
        for (const auto &update : updates)
            QVERIFY(!qvariant_cast<aha::Transcript>(update.first()).text.contains("<KEY>"));
        const auto payload = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(payload.value("model").toString(), QStringLiteral("AgenticASR-Refiner"));
        QCOMPARE(payload.value("store"), QJsonValue(false));
        QVERIFY(!request.toLower().contains("authorization:"));
    }
    void cancellationSuppressesLateEvents()
    {
        aha::CorrectionConfig config;
        config.enabled = true;
        config.responsesUrl = "http://127.0.0.1:1/v1/responses";
        aha::TextCorrection correction(config, false);
        QSignalSpy updates(&correction, &aha::TextCorrection::changed);
        correction.enqueue({"one", "原文", true, false, false, {}});
        QCOMPARE(updates.size(), 1);
        correction.cancel();
        QTest::qWait(30);
        QCOMPARE(updates.size(), 1);
    }
    void liveEngine()
    {
        const QString url = qEnvironmentVariable("AHA_FLOW_LIVE_CORRECTION_URL");
        if (url.isEmpty())
            QSKIP("真实纠错检查需显式设置 AHA_FLOW_LIVE_CORRECTION_URL");
        aha::CorrectionConfig config;
        config.enabled = true;
        config.responsesUrl = url;
        aha::TextCorrection correction(config, true);
        QSignalSpy updates(&correction, &aha::TextCorrection::changed);
        correction.enqueue({"live", "今天今天我们测试缓存。", true, false, false, {}});
        QTRY_VERIFY_WITH_TIMEOUT(
            updates.size() > 1 && !qvariant_cast<aha::Transcript>(updates.last().first()).correcting, 60000);
        const auto final = qvariant_cast<aha::Transcript>(updates.last().first());
        QVERIFY2(final.correctionError.isEmpty(), qPrintable(final.correctionError));
        QVERIFY(!final.text.isEmpty());
        QVERIFY(!final.text.contains("<KEY>"));
    }
};
QTEST_GUILESS_MAIN(CorrectionTest)
#include "correction_test.moc"
