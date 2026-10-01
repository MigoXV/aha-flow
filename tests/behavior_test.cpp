#include "core/settings.h"
#include "core/transcription.h"
#include "network/text_correction.h"
#include "network/proxy.h"
#include "audio/pcm_converter.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>

class BehaviorTest final : public QObject
{
    Q_OBJECT
  private slots:
    void settingsPersist()
    {
        QTemporaryDir directory;
        QSettings store(directory.filePath("settings.ini"), QSettings::IniFormat);
        auto settings = aha::AppSettings::load(store);
        QVERIFY(settings.cache.enabled);
        QCOMPARE(settings.cache.rawSliceSeconds, 300);
        QVERIFY(settings.engine.allowUntrustedCertificate);
        settings.cache.rootDirectory = directory.path();
        settings.cache.rawSliceSeconds = 17;
        settings.engine.language = "zh";
        settings.engine.apiKey = "local-test-key";
        settings.correction.enabled = true;
        settings.rememberServer();
        settings.save(store);
        const auto loaded = aha::AppSettings::load(store);
        QCOMPARE(loaded.cache.rootDirectory, directory.path());
        QCOMPARE(loaded.cache.rawSliceSeconds, 17);
        QCOMPARE(loaded.engine.apiKey, QStringLiteral("local-test-key"));
        QVERIFY(loaded.validate().isEmpty());
        settings.cache.rootDirectory = "relative";
        QVERIFY(!settings.validate().isEmpty());
        settings.cache.enabled = false;
        QVERIFY(settings.validate().isEmpty());
    }
    void semanticBoundaryAndInterim()
    {
        aha::TranscriptionState state;
        const auto event = [](const QString &type, const QString &id) {
            return QJsonObject{{"type", type}, {"item_id", id}};
        };
        state.apply(event("input_audio_buffer.speech_started", "one"));
        QCOMPARE(state.status(), QStringLiteral("聆听中"));
        auto preview = event("x_aha.input_audio_transcription.interim", "one");
        preview.insert("revision", 2);
        preview.insert("transcript", "预览");
        QVERIFY(state.apply(preview));
        QVERIFY(!state.apply(preview));
        state.apply(event("x_aha.vad.physical_speech_stopped", "one"));
        state.apply(event("input_audio_buffer.speech_started", "two"));
        state.apply(event("input_audio_buffer.speech_started", "one"));
        QCOMPARE(state.status(), QStringLiteral("语义轮次未结束"));
        state.apply(event("x_aha.input_audio_transcription.interim_cleared", "one"));
        preview.insert("revision", 3);
        QVERIFY(!state.apply(preview));
        auto delta = event("conversation.item.input_audio_transcription.delta", "one");
        delta.insert("delta", "正式");
        QCOMPARE(state.apply(delta)->text, QStringLiteral("正式"));
        QCOMPARE(state.status(), QStringLiteral("语义轮次未结束"));
        state.apply(event("x_aha.vad.turn_completed", "one"));
        QCOMPARE(state.status(), QStringLiteral("聆听中"));
        state.apply(event("conversation.item.input_audio_transcription.completed", "two"));
        QCOMPARE(state.status(), QStringLiteral("转写中"));
        auto final = state.apply(event("conversation.item.input_audio_transcription.completed", "one"));
        QCOMPARE(final->text, QStringLiteral("正式"));
        QVERIFY(final->final);
        QCOMPARE(state.status(), QStringLiteral("等待中"));
    }
    void sseBoundaries()
    {
        QList<QJsonObject> events;
        aha::SseParser parser([&](const QString &name, const QJsonObject &event) {
            QCOMPARE(name, QStringLiteral("sample"));
            events.append(event);
        });
        const QByteArray stream =
            QStringLiteral(
                ":comment\r\nevent: sample\r\ndata: {\"delta\":\"你好\",\r\ndata: \"type\":\"delta\"}\r\n\r\n")
                .toUtf8();
        for (char byte : stream)
            parser.push(QByteArray(1, byte));
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().value("delta").toString(), QStringLiteral("你好"));
        QCOMPARE(aha::correctionBody("正文<KE"), QStringLiteral("正文"));
        QCOMPARE(aha::correctionBody("正文<KEY>实体", true), QStringLiteral("正文"));
        QVERIFY_EXCEPTION_THROWN(parser.push("data: invalid\n\n"), std::runtime_error);
    }
    void originalVadReplay()
    {
        QFile fixture(QStringLiteral(AHA_FLOW_FIXTURES "/realtime-vad-replay.json"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const QJsonDocument document = QJsonDocument::fromJson(fixture.readAll());
        QVERIFY(document.isArray());
        aha::TranscriptionState state;
        int physicalStops = 0;
        int completedTurns = 0;
        for (const auto &value : document.array()) {
            const QJsonObject event = value.toObject();
            const QString type = event.value("type").toString();
            const auto result = state.apply(event);
            if (type == "input_audio_buffer.speech_started")
                QCOMPARE(state.status(), QStringLiteral("聆听中"));
            else if (type == "x_aha.vad.physical_speech_stopped") {
                ++physicalStops;
                QCOMPARE(state.status(), QStringLiteral("语义轮次未结束"));
            } else if (type == "x_aha.vad.physical_speech_started" && event.contains("item_id"))
                QCOMPARE(state.status(), QStringLiteral("语义轮次未结束"));
            else if (type == "x_aha.vad.turn_completed")
                QCOMPARE(state.status(), QStringLiteral("转写中"));
            else if (type == "conversation.item.input_audio_transcription.completed") {
                QVERIFY(result && result->final);
                ++completedTurns;
                QCOMPARE(state.status(), QStringLiteral("等待中"));
            }
        }
        QCOMPARE(physicalStops, 4);
        QCOMPARE(completedTurns, 2);
    }
    void proxyRules()
    {
        struct EnvironmentRestore {
            QList<QPair<QByteArray, QByteArray>> values;
            QSet<QByteArray> absent;
            EnvironmentRestore()
            {
                for (const QByteArray &name : {QByteArray("https_proxy"), QByteArray("no_proxy")}) {
                    if (!qEnvironmentVariableIsSet(name.constData()))
                        absent.insert(name);
                    values.append({name, qgetenv(name.constData())});
                }
            }
            ~EnvironmentRestore()
            {
                for (const auto &value : values)
                    if (absent.contains(value.first))
                        qunsetenv(value.first.constData());
                    else
                        qputenv(value.first.constData(), value.second);
            }
        } restore;
        qputenv("https_proxy", "http://user:password@localhost:3128");
        qputenv("no_proxy", "example.test:443,192.168.0.222");
        QCOMPARE(aha::engineProxy(QUrl("wss://sub.example.test/session")).type(), QNetworkProxy::NoProxy);
        QCOMPARE(aha::engineProxy(QUrl("https://192.168.0.222:10000")).type(), QNetworkProxy::NoProxy);
        const auto proxy = aha::engineProxy(QUrl("https://remote.test"));
        QCOMPARE(proxy.type(), QNetworkProxy::HttpProxy);
        QCOMPARE(proxy.port(), quint16(3128));
        QCOMPARE(proxy.user(), QStringLiteral("user"));
        qputenv("https_proxy", "socks5h://localhost:1080");
        QCOMPARE(aha::engineProxy(QUrl("wss://remote.test")).type(), QNetworkProxy::Socks5Proxy);
        qputenv("https_proxy", "https://localhost:3128");
        QVERIFY_EXCEPTION_THROWN(aha::engineProxy(QUrl("https://remote.test")), std::runtime_error);
    }
    void resamplingAndTail()
    {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        aha::PcmConverter converter(format);
        QByteArray input(48000 * 2 * 4, '\0');
        for (int frame = 0; frame < 48000; ++frame) {
            const float sample = static_cast<float>(0.5 * std::sin(2 * 3.141592653589793 * 440 * frame / 48000));
            for (int channel = 0; channel < 2; ++channel)
                std::memcpy(input.data() + (frame * 2 + channel) * 4, &sample, 4);
        }
        QByteArray output;
        for (int offset = 0; offset < input.size(); offset += 1001)
            for (const auto &chunk : converter.feed(input.mid(offset, 1001))) {
                QCOMPARE(chunk.size(), 4800);
                output += chunk;
            }
        for (const auto &chunk : converter.feed({}, true))
            output += chunk;
        QVERIFY(qAbs(output.size() - 48000) <= 2);
        QCOMPARE(output.size() % 2, 0);
        int maximum = 0;
        for (int i = 0; i < output.size(); i += 2)
            maximum = qMax(maximum, qAbs(static_cast<int>(qFromLittleEndian<qint16>(output.constData() + i))));
        QVERIFY(maximum > 15000 && maximum < 18000);
        QVERIFY(converter.feed({}, true).isEmpty());
    }
};
QTEST_GUILESS_MAIN(BehaviorTest)
#include "behavior_test.moc"
