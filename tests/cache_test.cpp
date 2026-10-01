#include "storage/audio_cache.h"
#include <FLAC/stream_decoder.h>
#include <QDir>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QRegularExpression>
#include <QtEndian>

namespace
{
FLAC__StreamDecoderWriteStatus decoded(const FLAC__StreamDecoder *, const FLAC__Frame *frame,
                                       const FLAC__int32 *const buffer[], void *client)
{
    auto *pcm = static_cast<QByteArray *>(client);
    for (unsigned i = 0; i < frame->header.blocksize; ++i) {
        char bytes[2];
        qToLittleEndian<qint16>(static_cast<qint16>(buffer[0][i]), bytes);
        pcm->append(bytes, 2);
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void decodeError(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *) {}
QByteArray decode(const QString &path)
{
    QByteArray pcm;
    FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
    if (FLAC__stream_decoder_init_file(decoder, path.toUtf8().constData(), decoded, nullptr, decodeError, &pcm) ==
        FLAC__STREAM_DECODER_INIT_STATUS_OK)
        FLAC__stream_decoder_process_until_end_of_stream(decoder);
    FLAC__stream_decoder_delete(decoder);
    return pcm;
}
QByteArray read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
} // namespace
class CacheTest final : public QObject
{
    Q_OBJECT
  private slots:
    void rawAndCrossSliceVad()
    {
        QTemporaryDir temporary;
        aha::CacheConfig config;
        config.rootDirectory = temporary.path();
        config.rawSliceSeconds = 1;
        aha::AudioCache cache("test", config);
        QSignalSpy errors(&cache, &aha::AudioCache::failed);
        cache.open();
        const QString raw = cache.rawDirectory();
        QVERIFY(!raw.isEmpty());
        QByteArray pcm(120000, '\0');
        for (int i = 0; i < pcm.size() / 2; ++i)
            qToLittleEndian<qint16>(static_cast<qint16>(i % 20000 - 10000), pcm.data() + i * 2);
        QVERIFY(cache.postPcm(pcm));
        QVERIFY(cache.postEvent(
            {{"type", "input_audio_buffer.speech_started"}, {"item_id", "turn"}, {"audio_start_ms", 900}}));
        QVERIFY(cache.postEvent({{"type", "x_aha.vad.turn_completed"}, {"item_id", "turn"}, {"audio_end_ms", 2100}}));
        QVERIFY(cache.postEvent({{"type", "conversation.item.input_audio_transcription.completed"},
                                 {"item_id", "turn"},
                                 {"transcript", "原始文字"}}));
        QMetaObject::invokeMethod(&cache, &aha::AudioCache::finish, Qt::QueuedConnection);
        QTRY_VERIFY(!QFile::exists(QDir(raw).filePath("pending.pcm")));
        QCOMPARE(errors.size(), 0);
        QCOMPARE(decode(QDir(raw).filePath("000001.flac")), pcm.left(48000));
        QCOMPARE(decode(QDir(raw).filePath("000002.flac")), pcm.mid(48000, 48000));
        QCOMPARE(decode(QDir(raw).filePath("000003.flac")), pcm.mid(96000));
        const QString vad = QDir(temporary.path()).filePath("vad/" + QFileInfo(raw).fileName());
        QCOMPARE(decode(QDir(vad).filePath("000001.flac")), pcm);
        QVERIFY(read(QDir(vad).filePath("metadata.jsonl")).contains(QStringLiteral("原始文字").toUtf8()));
        const auto rows = read(QDir(raw).filePath("metadata.jsonl")).trimmed().split('\n');
        QCOMPARE(rows.size(), 3);
        QVERIFY(QJsonDocument::fromJson(rows[1]).object().value("seconds").isArray());
    }
    void missingBoundsAndCollision()
    {
        QTemporaryDir temporary;
        aha::CacheConfig config;
        config.rootDirectory = temporary.path();
        aha::AudioCache first("one", config), second("two", config);
        first.open();
        second.open();
        QVERIFY(first.rawDirectory() != second.rawDirectory());
        QVERIFY(first.postPcm(QByteArray(1000, '\0')));
        QVERIFY(first.postEvent({{"type", "conversation.item.input_audio_transcription.completed"},
                                 {"item_id", "missing"},
                                 {"transcript", "无边界"}}));
        QMetaObject::invokeMethod(&first, &aha::AudioCache::finish, Qt::QueuedConnection);
        QTRY_VERIFY(QFile::exists(QDir(first.rawDirectory()).filePath("000001.flac")));
        QCOMPARE(QDir(QDir(temporary.path()).filePath("vad/" + QFileInfo(first.rawDirectory()).fileName()))
                     .entryList({"*.flac"})
                     .size(),
                 0);
        second.finish();
    }
    void disabledAndInvalidDirectory()
    {
        QTemporaryDir temporary;
        aha::CacheConfig config;
        config.rootDirectory = temporary.filePath("disabled");
        config.enabled = false;
        aha::AudioCache disabled("disabled", config);
        disabled.open();
        QVERIFY(disabled.postPcm("odd"));
        disabled.finish();
        QVERIFY(!QDir(config.rootDirectory).exists());
        QFile file(temporary.filePath("file"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        config.enabled = true;
        config.rootDirectory = file.fileName();
        aha::AudioCache invalid("invalid", config);
        QSignalSpy errors(&invalid, &aha::AudioCache::failed);
        invalid.open();
        QCOMPARE(errors.size(), 1);
        invalid.finish();
    }
    void boundedQueueAndRetainedSpool()
    {
        QTemporaryDir temporary;
        aha::CacheConfig config;
        config.rootDirectory = temporary.path();
        aha::AudioCache cache("limit", config);
        cache.open();
        for (int i = 0; i < 5; ++i)
            QVERIFY(cache.postPcm(QByteArray(48000, '\0')));
        QVERIFY(!cache.postPcm(QByteArray(2, '\0')));
        QCoreApplication::processEvents();
        QSignalSpy errors(&cache, &aha::AudioCache::failed);
        QVERIFY(cache.postPcm("odd"));
        QTRY_COMPARE(errors.size(), 1);
        cache.finish();
        QVERIFY(QFile::exists(QDir(cache.rawDirectory()).filePath("pending.pcm")));
        QCOMPARE(QFileInfo(QDir(cache.rawDirectory()).filePath("pending.pcm")).size(), 240000);
    }
    void longRecordingMemory()
    {
#ifdef Q_OS_LINUX
        if (qEnvironmentVariable("AHA_FLOW_LONG_CACHE_TEST") != "1")
            QSKIP("显式设置 AHA_FLOW_LONG_CACHE_TEST=1 执行一小时缓存压力测试");
        const auto rss = [] {
            return QRegularExpression(QStringLiteral("VmRSS:\\s*(\\d+)"))
                .match(QString::fromUtf8(read("/proc/self/status")))
                .captured(1)
                .toLongLong();
        };
        QTemporaryDir temporary;
        aha::CacheConfig config;
        config.rootDirectory = temporary.path();
        aha::AudioCache cache("long", config);
        QSignalSpy errors(&cache, &aha::AudioCache::failed);
        cache.open();
        qint64 warm = 0, middle = 0;
        for (int seconds = 5; seconds <= 3600; seconds += 5) {
            QVERIFY(cache.postPcm(QByteArray(240000, '\0')));
            QCoreApplication::processEvents();
            if (seconds == 300)
                warm = rss();
            if (seconds == 1800)
                middle = rss();
        }
        cache.finish();
        QCOMPARE(errors.size(), 0);
        const qint64 final = rss();
        qInfo("cache RSS KiB: after 300s=%lld, 1800s=%lld, 3600s=%lld", warm, middle, final);
        QVERIFY(final - warm < 8192);
        QVERIFY(!QFile::exists(QDir(cache.rawDirectory()).filePath("pending.pcm")));
#else
        QSKIP("Linux 专用 RSS 压力测试");
#endif
    }
};
QTEST_GUILESS_MAIN(CacheTest)
#include "cache_test.moc"
