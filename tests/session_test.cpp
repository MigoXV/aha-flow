#include "session/session_controller.h"
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWebSocketServer>
#include <QWebSocket>

class SessionTest final : public QObject
{
    Q_OBJECT
  private slots:
    void replayStopAndRestart()
    {
        QTemporaryDir directory;
        QFile pcm(directory.filePath("input.pcm"));
        QVERIFY(pcm.open(QIODevice::WriteOnly));
        pcm.write(QByteArray(96000, '\0'));
        pcm.close();
        QWebSocketServer server("test", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int chunks = 0, sessions = 0;
        QJsonObject update;
        connect(&server, &QWebSocketServer::newConnection, &server, [&] {
            QWebSocket *socket = server.nextPendingConnection();
            ++sessions;
            connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QWebSocket::textMessageReceived, socket, [&, socket](const QString &text) {
                const auto event = QJsonDocument::fromJson(text.toUtf8()).object();
                const auto send = [socket](const QJsonObject &message) {
                    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
                };
                if (event.value("type") == "session.update") {
                    update = event;
                    send({{"type", "session.updated"}});
                }
                if (event.value("type") == "input_audio_buffer.append") {
                    ++chunks;
                    QCOMPARE(QByteArray::fromBase64(event.value("audio").toString().toLatin1()).size(), 4800);
                    send({{"type", "input_audio_buffer.speech_started"}, {"item_id", "turn"}, {"audio_start_ms", 0}});
                    send({{"type", "input_audio_buffer.speech_stopped"}, {"item_id", "turn"}, {"audio_end_ms", 100}});
                    send({{"type", "conversation.item.input_audio_transcription.completed"},
                          {"item_id", "turn"},
                          {"transcript", "原始转写"}});
                }
            });
            socket->sendTextMessage("{\"type\":\"session.created\"}");
        });
        aha::AppSettings settings;
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        settings.cache.rootDirectory = directory.filePath("first");
        aha::SessionController controller;
        QSignalSpy transcript(&controller, &aha::SessionController::transcriptChanged),
            finished(&controller, &aha::SessionController::finished);
        QSignalSpy cache(&controller, &aha::SessionController::cacheDirectory),
            closing(&controller, &aha::SessionController::shutdownReady);
        controller.start(settings, pcm.fileName());
        settings.cache.rootDirectory = directory.filePath("second");
        QTRY_VERIFY_WITH_TIMEOUT(transcript.size() > 0, 5000);
        QCOMPARE(update.value("session").toObject().value("type").toString(), QStringLiteral("transcription"));
        QVERIFY(cache.at(0).at(0).toString().startsWith(directory.filePath("first")));
        controller.stop();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!QFile::exists(QDir(cache.at(0).at(0).toString()).filePath("pending.pcm")));
        controller.start(settings, pcm.fileName());
        QTRY_COMPARE_WITH_TIMEOUT(cache.size(), 2, 5000);
        QTRY_VERIFY(sessions == 2);
        QVERIFY(cache.at(1).at(0).toString().startsWith(directory.filePath("second")));
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closing.size(), 1, 5000);
    }
    void manualStopWaitsForFinalResult_data()
    {
        QTest::addColumn<bool>("respond");
        QTest::newRow("final-result") << true;
        QTest::newRow("timeout") << false;
    }
    void manualStopWaitsForFinalResult()
    {
        QFETCH(bool, respond);
        QTemporaryDir directory;
        QFile pcm(directory.filePath("manual.pcm"));
        QVERIFY(pcm.open(QIODevice::WriteOnly));
        pcm.write(QByteArray(9600, '\0'));
        pcm.close();
        QWebSocketServer server("manual", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int commits = 0;
        connect(&server, &QWebSocketServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QWebSocket::textMessageReceived, socket, [&, socket](const QString &text) {
                const auto event = QJsonDocument::fromJson(text.toUtf8()).object();
                const auto send = [socket](QJsonObject event) {
                    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson()));
                };
                if (event.value("type") == "session.update")
                    send({{"type", "session.updated"}, {"session", event.value("session")}});
                if (event.value("type") == "input_audio_buffer.commit") {
                    ++commits;
                    send({{"type", "input_audio_buffer.committed"}, {"item_id", "manual"}});
                    if (respond)
                        QTimer::singleShot(80, socket, [send] {
                            send({{"type", "conversation.item.input_audio_transcription.completed"},
                                  {"item_id", "manual"},
                                  {"transcript", "手动提交最终结果"}});
                        });
                }
            });
            socket->sendTextMessage("{\"type\":\"session.created\"}");
        });
        aha::AppSettings settings;
        settings.cache.enabled = false;
        settings.engine.baseUrl = QString("http://127.0.0.1:%1").arg(server.serverPort());
        settings.engine.realtimeOptions = {{"turn_detection", QJsonValue::Null}};
        aha::SessionController controller(nullptr, 200);
        QSignalSpy status(&controller, &aha::SessionController::statusChanged);
        QSignalSpy results(&controller, &aha::SessionController::transcriptChanged);
        QSignalSpy finished(&controller, &aha::SessionController::finished);
        controller.start(settings, pcm.fileName());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(commits, 1);
        if (respond) {
            QCOMPARE(results.size(), 1);
            QCOMPARE(qvariant_cast<aha::Transcript>(results.first().first()).text, QString("手动提交最终结果"));
        } else {
            QCOMPARE(results.size(), 0);
            QCOMPARE(status.last().first().toString(), QString("错误"));
            QVERIFY(status.last()[1].toString().contains(QStringLiteral("超时")));
        }
    }
    void cancelWhileConnecting()
    {
        aha::SessionController controller;
        aha::AppSettings settings;
        settings.cache.enabled = false;
        settings.engine.baseUrl = "http://127.0.0.1:1";
        QSignalSpy finished(&controller, &aha::SessionController::finished),
            closing(&controller, &aha::SessionController::shutdownReady);
        controller.start(settings);
        controller.stop();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        controller.shutdown();
        QTRY_COMPARE(closing.size(), 1);
    }
};
QTEST_GUILESS_MAIN(SessionTest)
#include "session_test.moc"
