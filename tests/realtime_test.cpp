#include "network/realtime_client.h"
#include "core/transcription.h"
#include <QJsonArray>
#include <QPointer>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocketServer>
#include <QWebSocket>

namespace
{
void send(QWebSocket *socket, const QJsonObject &event)
{
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));
}
} // namespace
class RealtimeTest final : public QObject
{
    Q_OBJECT
  private slots:
    void liveOfficialConfiguration()
    {
        if (!qEnvironmentVariableIsSet("AHA_FLOW_LIVE_REALTIME"))
            QSKIP("Explicit live API opt-in required");
        const QString key = qEnvironmentVariable("AHA_FLOW_API_KEY");
        QVERIFY2(!key.isEmpty(), "AHA_FLOW_API_KEY is required");
        const QStringList models{"whisper-1",         "gpt-4o-mini-transcribe",    "gpt-4o-mini-transcribe-2025-12-15",
                                 "gpt-4o-transcribe", "gpt-4o-transcribe-diarize", "gpt-live-transcribe",
                                 "gpt-transcribe",    "gpt-realtime-whisper"};
        const QList<QPair<QString, QJsonObject>> cases{
            {"baseline", {}},
            {"server_vad", {{"turn_detection", QJsonObject{{"type", "server_vad"}}}}},
            {"semantic_vad", {{"turn_detection", QJsonObject{{"type", "semantic_vad"}, {"eagerness", "high"}}}}},
            {"vad_numeric",
             {{"turn_detection", QJsonObject{{"type", "server_vad"},
                                             {"threshold", 0.6},
                                             {"prefix_padding_ms", 300},
                                             {"silence_duration_ms", 500}}}}},
            {"manual", {{"turn_detection", QJsonValue::Null}}},
            {"prompt", {{"transcription", QJsonObject{{"prompt", "Software engineering discussion."}}}}},
            {"keywords", {{"transcription", QJsonObject{{"keywords", QJsonArray{"Qt", "CMake"}}}}}},
            {"delay", {{"transcription", QJsonObject{{"delay", "low"}}}}},
            {"near_field", {{"noise_reduction", QJsonObject{{"type", "near_field"}}}}},
            {"far_field_strength", {{"noise_reduction", QJsonObject{{"type", "far_field"}, {"strength", 0.5}}}}},
            {"noise_off", {{"noise_reduction", QJsonValue::Null}}},
            {"logprobs", {{"logprobs", true}}}};
        int accepted = 0;
        for (const auto &model : models) {
            bool baseline = false;
            for (const auto &test : cases) {
                aha::EngineConfig config;
                config.baseUrl = "https://api.openai.com";
                config.model = model;
                config.apiKey = key;
                config.allowUntrustedCertificate = false;
                config.realtimeOptions = test.second;
                aha::RealtimeClient client;
                QSignalSpy ready(&client, &aha::RealtimeClient::ready), errors(&client, &aha::RealtimeClient::failed);
                client.start(config);
                QElapsedTimer wait;
                wait.start();
                while (ready.isEmpty() && errors.isEmpty() && wait.elapsed() < 12000)
                    QTest::qWait(20);
                QVERIFY2(!ready.isEmpty() || !errors.isEmpty(), "No API outcome within 12 seconds");
                if (!ready.isEmpty()) {
                    ++accepted;
                    qInfo().noquote() << model << test.first << "accepted"
                                      << QJsonDocument(client.effectiveConfiguration()).toJson(QJsonDocument::Compact);
                } else {
                    qInfo().noquote() << model << test.first << "rejected:" << errors.first().first().toString();
                }
                if (test.first == "baseline")
                    baseline = !ready.isEmpty();
                client.stop();
                if (!baseline)
                    break;
            }
        }
        QVERIFY2(
            accepted > 0,
            "No official model/configuration was accepted; inspect authentication, network or model access errors");
    }
    void liveOfficialAudio_data()
    {
        QTest::addColumn<bool>("manual");
        QTest::newRow("server_vad") << false;
        QTest::newRow("manual_commit") << true;
    }
    void liveOfficialAudio()
    {
        if (!qEnvironmentVariableIsSet("AHA_FLOW_LIVE_REALTIME"))
            QSKIP("Explicit live API opt-in required");
        QFETCH(bool, manual);
        QFile pcm(qEnvironmentVariable("AHA_FLOW_LIVE_PCM"));
        if (pcm.fileName().isEmpty())
            QSKIP("AHA_FLOW_LIVE_PCM must point to a short 24kHz mono PCM16LE fixture");
        QVERIFY(pcm.open(QIODevice::ReadOnly));
        aha::EngineConfig config;
        config.baseUrl = "https://api.openai.com";
        config.model = qEnvironmentVariable("AHA_FLOW_LIVE_MODEL", "gpt-4o-mini-transcribe");
        config.apiKey = qEnvironmentVariable("AHA_FLOW_API_KEY");
        config.allowUntrustedCertificate = false;
        config.realtimeOptions = {{"turn_detection", manual ? QJsonValue(QJsonValue::Null)
                                                            : QJsonValue(QJsonObject{{"type", "server_vad"}})}};
        aha::RealtimeClient client;
        QSignalSpy ready(&client, &aha::RealtimeClient::ready), errors(&client, &aha::RealtimeClient::failed);
        QString finalText;
        connect(&client, &aha::RealtimeClient::protocolEvent, &client, [&](const QJsonObject &event) {
            if (event.value("type") == "conversation.item.input_audio_transcription.completed")
                finalText += event.value("transcript").toString();
        });
        client.start(config);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !errors.isEmpty(), 12000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first().first().toString()));
        while (!pcm.atEnd() && errors.isEmpty()) {
            client.sendPcm(pcm.read(4800));
            QTest::qWait(100);
        }
        if (manual)
            QVERIFY(client.commit());
        QTRY_VERIFY_WITH_TIMEOUT(!finalText.trimmed().isEmpty() || !errors.isEmpty(), 30000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first().first().toString()));
        QVERIFY(!finalText.trimmed().isEmpty());
        qInfo().noquote() << (manual ? "manual" : "server_vad") << "final transcript:" << finalText;
        client.stop();
    }
    void optionalFieldsAndModelIndependence()
    {
        aha::EngineConfig config;
        auto input =
            aha::sessionUpdate(config).value("session").toObject().value("audio").toObject().value("input").toObject();
        QVERIFY(!input.contains("turn_detection"));
        QVERIFY(!input.contains("noise_reduction"));
        config.realtimeOptions = {
            {"turn_detection", QJsonObject{{"type", "semantic_vad"}, {"eagerness", "high"}}},
            {"noise_reduction", QJsonValue::Null},
            {"transcription", QJsonObject{{"prompt", "术语"}, {"keywords", QJsonArray{"Qt"}}, {"delay", "low"}}},
            {"logprobs", true}};
        config.language = "zh,en";
        config.languageMode = "multiple";
        for (const auto &model : {"default-audio", "whisper-1", "gpt-live-transcribe", "unknown-model"}) {
            config.model = model;
            const auto session = aha::sessionUpdate(config).value("session").toObject();
            input = session.value("audio").toObject().value("input").toObject();
            QCOMPARE(input.value("turn_detection").toObject().value("eagerness").toString(), QString("high"));
            QVERIFY(input.value("noise_reduction").isNull());
            const auto transcription = input.value("transcription").toObject();
            QCOMPARE(transcription.value("model").toString(), QString(model));
            QCOMPARE(transcription.value("languages").toArray(), QJsonArray({"zh", "en"}));
            QVERIFY(!transcription.contains("language"));
            QCOMPARE(transcription.value("prompt").toString(), QString("术语"));
            QCOMPARE(session.value("include").toArray(), QJsonArray({"item.input_audio_transcription.logprobs"}));
        }
        config.languageMode = "omit";
        config.realtimeOptions = {{"turn_detection", QJsonValue::Null}};
        input =
            aha::sessionUpdate(config).value("session").toObject().value("audio").toObject().value("input").toObject();
        QVERIFY(input.value("turn_detection").isNull());
        QVERIFY(!input.value("transcription").toObject().contains("languages"));
        QVERIFY(!input.value("transcription").toObject().contains("language"));
    }
    void commitsPreserveFollowingAudioAndResetOnRestart()
    {
        QWebSocketServer server("test", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QPointer<QWebSocket> peer;
        int commits = 0;
        connect(&server, &QWebSocketServer::newConnection, &server, [&] {
            peer = server.nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            connect(peer, &QWebSocket::textMessageReceived, peer, [&](const QString &text) {
                const auto event = QJsonDocument::fromJson(text.toUtf8()).object();
                if (event.value("type") == "session.update")
                    send(peer,
                         {{"type", "session.updated"},
                          {"session",
                           QJsonObject{
                               {"type", "transcription"},
                               {"audio", QJsonObject{{"input", QJsonObject{{"turn_detection", QJsonValue::Null}}}}}}}});
                if (event.value("type") == "input_audio_buffer.commit")
                    ++commits;
            });
            send(peer, {{"type", "session.created"}});
        });
        aha::EngineConfig config;
        config.baseUrl = QString("http://127.0.0.1:%1").arg(server.serverPort());
        aha::RealtimeClient client;
        QSignalSpy ready(&client, &aha::RealtimeClient::ready), errors(&client, &aha::RealtimeClient::failed);
        client.start(config);
        QTRY_COMPARE(ready.size(), 1);
        QVERIFY(client.manualTurns());
        QVERIFY(!client.commit());
        client.sendPcm(QByteArray(4800, '\0'));
        QVERIFY(client.commit());
        QVERIFY(!client.commit());
        QTRY_COMPARE(commits, 1);
        client.sendPcm(QByteArray(4800, '\0'));
        send(peer, {{"type", "input_audio_buffer.committed"}, {"item_id", "first"}});
        send(peer, {{"type", "conversation.item.input_audio_transcription.completed"},
                    {"item_id", "first"},
                    {"transcript", "一"}});
        QTRY_COMPARE(client.uncommittedBytes(), qint64(4800));
        QVERIFY(client.hasPending());
        QVERIFY(client.commit());
        QTRY_COMPARE(commits, 2);
        send(peer, {{"type", "input_audio_buffer.committed"}, {"item_id", "second"}});
        send(peer, {{"type", "conversation.item.input_audio_transcription.completed"},
                    {"item_id", "second"},
                    {"transcript", "二"}});
        QTRY_VERIFY(!client.hasPending());
        QCOMPARE(errors.size(), 0);
        client.stop();
        QTRY_VERIFY(peer == nullptr || peer->state() == QAbstractSocket::UnconnectedState);
        client.start(config);
        QTRY_COMPARE(ready.size(), 2);
        QVERIFY(!client.hasPending());
        client.stop();
    }
    void rejectedParameterCanBeRemoved()
    {
        QWebSocketServer server("test", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QWebSocketServer::newConnection, &server, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            connect(peer, &QWebSocket::textMessageReceived, peer, [peer](const QString &text) {
                const auto event = QJsonDocument::fromJson(text.toUtf8()).object();
                if (event.value("type") != "session.update")
                    return;
                const auto session = event.value("session").toObject();
                if (session.value("audio").toObject().value("input").toObject().contains("turn_detection"))
                    send(peer, {{"type", "error"},
                                {"error", QJsonObject{{"type", "invalid_request_error"},
                                                      {"code", "unsupported_parameter"},
                                                      {"param", "session.audio.input.turn_detection"},
                                                      {"message", "unsupported; private-test-key"}}}});
                else
                    send(peer, {{"type", "session.updated"}, {"session", session}});
            });
            send(peer, {{"type", "session.created"}});
        });
        aha::EngineConfig config;
        config.baseUrl = QString("http://127.0.0.1:%1").arg(server.serverPort());
        config.apiKey = "private-test-key";
        config.realtimeOptions = {{"turn_detection", QJsonObject{{"type", "semantic_vad"}}}};
        aha::RealtimeClient client;
        QSignalSpy errors(&client, &aha::RealtimeClient::failed), ready(&client, &aha::RealtimeClient::ready);
        client.start(config);
        QTRY_COMPARE(errors.size(), 1);
        const auto error = errors.first().first().toString();
        QVERIFY(error.contains("unsupported_parameter"));
        QVERIFY(error.contains("session.audio.input.turn_detection"));
        QVERIFY(!error.contains(config.apiKey));
        config.realtimeOptions = {};
        client.start(config);
        QTRY_COMPARE(ready.size(), 1);
        client.stop();
    }
};
QTEST_GUILESS_MAIN(RealtimeTest)
#include "realtime_test.moc"
