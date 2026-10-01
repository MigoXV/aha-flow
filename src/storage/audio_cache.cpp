#include "storage/audio_cache.h"
#include <FLAC/stream_encoder.h>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace aha
{
namespace
{
void require(bool okay, const QString &message)
{
    if (!okay)
        throw std::runtime_error(message.toStdString());
}
FLAC__StreamEncoderWriteStatus writeFlac(const FLAC__StreamEncoder *, const FLAC__byte bytes[], size_t count, unsigned,
                                         unsigned, void *client)
{
    auto *file = static_cast<QFile *>(client);
    return file->write(reinterpret_cast<const char *>(bytes), static_cast<qint64>(count)) == static_cast<qint64>(count)
               ? FLAC__STREAM_ENCODER_WRITE_STATUS_OK
               : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}
qint64 sampleTime(const QJsonValue &value)
{
    const double ms = value.toDouble(-1);
    return value.isDouble() && std::isfinite(ms) && ms >= 0 && ms < 1e12 ? qRound64(ms * 24) : -1;
}
} // namespace
AudioCache::AudioCache(QString id, CacheConfig config) : id_(std::move(id)), config_(std::move(config))
{
    spool_.setParent(this);
}

void AudioCache::guard(const std::function<void()> &operation)
{
    if (failed_)
        return;
    try {
        operation();
    } catch (const std::exception &error) {
        failed_ = true;
        emit failed(id_, QString::fromUtf8(error.what()));
    }
}

void AudioCache::open()
{
    guard([this] {
        if (!config_.enabled) {
            emit opened(id_, {});
            return;
        }
        require(QDir::isAbsolutePath(config_.rootDirectory), QStringLiteral("缓存目录必须是绝对路径"));
        require(config_.rawSliceSeconds >= 1 && config_.rawSliceSeconds <= 3600, QStringLiteral("无效的缓存切片时长"));
        QDir root(config_.rootDirectory);
        require(root.mkpath("raw") && root.mkpath("vad"), QStringLiteral("无法创建缓存目录"));
        const QString base = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        for (int suffix = 0;; ++suffix) {
            const QString name = base + (suffix ? QStringLiteral("_%1").arg(suffix) : QString());
            if (!root.mkdir("raw/" + name)) {
                require(root.exists("raw/" + name), QStringLiteral("原始录音目录不可写"));
                continue;
            }
            if (!root.mkdir("vad/" + name)) {
                root.rmdir("raw/" + name);
                require(root.exists("vad/" + name), QStringLiteral("VAD 目录不可写"));
                continue;
            }
            rawDirectory_ = root.filePath("raw/" + name);
            vadDirectory_ = root.filePath("vad/" + name);
            break;
        }
        spool_.setFileName(QDir(rawDirectory_).filePath("pending.pcm"));
        require(spool_.open(QIODevice::ReadWrite | QIODevice::NewOnly), spool_.errorString());
        appendJson(QDir(rawDirectory_).filePath("session.json"),
                   {{"sample_rate", 24000},
                    {"channels", 1},
                    {"bits_per_sample", 16},
                    {"started_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
        for (const QString &directory : {rawDirectory_, vadDirectory_}) {
            QFile metadata(QDir(directory).filePath("metadata.jsonl"));
            require(metadata.open(QIODevice::WriteOnly | QIODevice::NewOnly), metadata.errorString());
        }
        emit opened(id_, rawDirectory_);
    });
}

bool AudioCache::postPcm(const QByteArray &pcm)
{
    if (!config_.enabled)
        return true;
    const qint64 bytes = pcm.size();
    if (pendingPcm_.fetch_add(bytes) + bytes > 240000) {
        pendingPcm_.fetch_sub(bytes);
        return false;
    }
    QMetaObject::invokeMethod(
        this,
        [this, pcm, bytes] {
            guard([this, &pcm] {
                if (!closed_)
                    append(pcm);
            });
            pendingPcm_.fetch_sub(bytes);
        },
        Qt::QueuedConnection);
    return true;
}
bool AudioCache::postEvent(const QJsonObject &message)
{
    if (!config_.enabled)
        return true;
    if (pendingEvents_.fetch_add(1) >= 1000) {
        pendingEvents_.fetch_sub(1);
        return false;
    }
    QMetaObject::invokeMethod(
        this,
        [this, message] {
            guard([this, &message] {
                if (!closed_)
                    event(message);
            });
            pendingEvents_.fetch_sub(1);
        },
        Qt::QueuedConnection);
    return true;
}

void AudioCache::appendJson(const QString &path, const QJsonObject &row)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Append), file.errorString());
    const QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
    require(file.write(bytes) == bytes.size() && file.flush(), file.errorString());
}

void AudioCache::append(const QByteArray &pcm)
{
    require(pcm.size() % 2 == 0, QStringLiteral("PCM16 块长度必须是偶数"));
    require(spool_.seek(totalSamples_ * 2), spool_.errorString());
    qint64 offset = 0;
    while (offset < pcm.size()) {
        const qint64 written = spool_.write(pcm.constData() + offset, pcm.size() - offset);
        require(written > 0, spool_.errorString());
        offset += written;
    }
    require(spool_.flush(), spool_.errorString());
    totalSamples_ += pcm.size() / 2;
    const qint64 sliceSamples = config_.rawSliceSeconds * 24000LL;
    while (totalSamples_ - rawEnd_ >= sliceSamples)
        saveRaw(rawEnd_ + sliceSamples);
    for (auto it = turns_.begin(); it != turns_.end();) {
        saveVad(it.value());
        if (it->saved) {
            savedItems_.insert(it.key());
            savedOrder_.enqueue(it.key());
            if (savedOrder_.size() > 1000)
                savedItems_.remove(savedOrder_.dequeue());
            it = turns_.erase(it);
        } else
            ++it;
    }
}

void AudioCache::event(const QJsonObject &message)
{
    QJsonObject log{{"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    log.insert("stage", "protocol-received");
    for (const QString &key : {QStringLiteral("type"), QStringLiteral("event_id"), QStringLiteral("item_id"),
                               QStringLiteral("audio_start_ms"), QStringLiteral("audio_end_ms"),
                               QStringLiteral("turn_completed"), QStringLiteral("stage")})
        if (message.contains(key))
            log.insert(key, message.value(key));
    appendJson(QDir(rawDirectory_).filePath("events.jsonl"), log);
    const QString id = message.value("item_id").toString();
    const QString type = message.value("type").toString();
    if (id.isEmpty() || message.contains("stage") || savedItems_.contains(id))
        return;
    require(turns_.contains(id) || turns_.size() < 1000, QStringLiteral("待保存 VAD 轮次过多"));
    Turn &turn = turns_[id];
    const qint64 start = sampleTime(message.value("audio_start_ms"));
    const qint64 end = sampleTime(message.value("audio_end_ms"));
    if (type == "input_audio_buffer.speech_started" && turn.start < 0)
        turn.start = start;
    const bool physical = type == "x_aha.vad.physical_speech_stopped" &&
                          (!message.contains("turn_completed") || message.value("turn_completed") == QJsonValue(false));
    const bool stopped = type == "input_audio_buffer.speech_stopped" || type == "x_aha.vad.turn_completed";
    std::optional<QPair<qint64, qint64>> boundary;
    if (physical || stopped) {
        if (turn.start < 0)
            turn.start = start;
        if (stopped && end >= 0)
            turn.end = end;
        const qint64 begin = physical ? start : turn.start;
        if (begin >= 0 && end > begin) {
            boundary = QPair<qint64, qint64>{begin, end};
            appendJson(QDir(rawDirectory_).filePath("boundaries.jsonl"), {{"start", begin}, {"end", end}});
        }
    }
    if (type == "conversation.item.input_audio_transcription.completed")
        turn.text = message.value("transcript").toString().trimmed();
    saveVad(turn);
    if (turn.saved || type == "conversation.item.input_audio_transcription.failed") {
        turns_.remove(id);
        savedItems_.insert(id);
        savedOrder_.enqueue(id);
        if (savedOrder_.size() > 1000)
            savedItems_.remove(savedOrder_.dequeue());
    }
    if (boundary)
        rawMetadata(boundary);
}

void AudioCache::encode(const QString &directory, const QString &name, qint64 start, qint64 end)
{
    const QString path = QDir(directory).filePath(name);
    QFile output(path + ".tmp");
    require(output.open(QIODevice::WriteOnly | QIODevice::Truncate), output.errorString());
    std::unique_ptr<FLAC__StreamEncoder, decltype(&FLAC__stream_encoder_delete)> encoder(FLAC__stream_encoder_new(),
                                                                                         FLAC__stream_encoder_delete);
    require(bool(encoder), QStringLiteral("无法创建 FLAC 编码器"));
    FLAC__stream_encoder_set_channels(encoder.get(), 1);
    FLAC__stream_encoder_set_bits_per_sample(encoder.get(), 16);
    FLAC__stream_encoder_set_sample_rate(encoder.get(), 24000);
    FLAC__stream_encoder_set_compression_level(encoder.get(), 5);
    FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), static_cast<FLAC__uint64>(end - start));
    require(FLAC__stream_encoder_init_stream(encoder.get(), writeFlac, nullptr, nullptr, nullptr, &output) ==
                FLAC__STREAM_ENCODER_INIT_STATUS_OK,
            QStringLiteral("无法初始化 FLAC 输出"));
    require(spool_.seek(start * 2), spool_.errorString());
    std::vector<FLAC__int32> samples(32768);
    for (qint64 position = start; position < end;) {
        const int count = static_cast<int>(std::min<qint64>(32768, end - position));
        const QByteArray bytes = spool_.read(count * 2);
        require(bytes.size() == count * 2, QStringLiteral("PCM 暂存文件不完整"));
        for (int i = 0; i < count; ++i)
            samples[static_cast<size_t>(i)] = qFromLittleEndian<qint16>(bytes.constData() + i * 2);
        require(FLAC__stream_encoder_process_interleaved(encoder.get(), samples.data(), static_cast<unsigned>(count)),
                QStringLiteral("FLAC 编码失败"));
        position += count;
    }
    require(FLAC__stream_encoder_finish(encoder.get()) && output.flush(), QStringLiteral("FLAC 收尾失败"));
    output.close();
    require(QFile::rename(output.fileName(), path), QStringLiteral("FLAC 文件提交失败"));
}

void AudioCache::saveRaw(qint64 end)
{
    const QString name = QStringLiteral("%1.flac").arg(rawSequence_ + 1, 6, 10, QLatin1Char('0'));
    encode(rawDirectory_, name, rawEnd_, end);
    appendJson(QDir(rawDirectory_).filePath("raw-slices.jsonl"),
               {{"file_name", name}, {"start", rawEnd_}, {"end", end}});
    ++rawSequence_;
    rawEnd_ = end;
    rawMetadata();
}

void AudioCache::saveVad(Turn &turn)
{
    if (turn.saved || turn.text.isEmpty() || turn.start < 0 || turn.end <= turn.start || turn.end > totalSamples_)
        return;
    const QString number = QStringLiteral("%1").arg(vadSequence_ + 1, 6, 10, QLatin1Char('0'));
    encode(vadDirectory_, number + ".flac", std::max<qint64>(0, turn.start - 24000),
           std::min(totalSamples_, turn.end + 24000));
    appendJson(QDir(vadDirectory_).filePath("metadata.jsonl"),
               {{"file_name", number + ".flac"}, {"id", "vad_" + number}, {"text", turn.text}});
    ++vadSequence_;
    turn.saved = true;
}

void AudioCache::rawMetadata(std::optional<QPair<qint64, qint64>> boundary)
{
    QFile previous(QDir(rawDirectory_).filePath("metadata.jsonl"));
    require(previous.open(QIODevice::ReadOnly), previous.errorString());
    QSaveFile file(QDir(rawDirectory_).filePath("metadata.jsonl"));
    require(file.open(QIODevice::WriteOnly), file.errorString());
    QFile index(QDir(rawDirectory_).filePath("raw-slices.jsonl"));
    if (!index.exists()) {
        previous.close();
        require(file.commit(), file.errorString());
        return;
    }
    require(index.open(QIODevice::ReadOnly), index.errorString());
    while (!index.atEnd()) {
        const QJsonObject entry = QJsonDocument::fromJson(index.readLine()).object();
        const Slice slice{entry.value("file_name").toString(), entry.value("start").toInteger(),
                          entry.value("end").toInteger()};
        const QByteArray previousLine = previous.readLine();
        const auto old = QJsonDocument::fromJson(previousLine).object();
        const bool exists = old.value("file_name").toString() == slice.name;
        if (exists && (!boundary || boundary->first >= slice.end || boundary->second <= slice.start)) {
            require(file.write(previousLine) == previousLine.size(), file.errorString());
            continue;
        }
        QList<QPair<qint64, qint64>> intervals;
        if (exists) {
            for (const QJsonValue &value : old.value("seconds").toArray()) {
                const QJsonArray interval = value.toArray();
                intervals.append({slice.start + qRound64(interval.at(0).toDouble() * 24000),
                                  slice.start + qRound64(interval.at(1).toDouble() * 24000)});
            }
            intervals.append({std::max(slice.start, boundary->first), std::min(slice.end, boundary->second)});
        } else {
            QFile boundaries(QDir(rawDirectory_).filePath("boundaries.jsonl"));
            if (boundaries.exists())
                require(boundaries.open(QIODevice::ReadOnly), boundaries.errorString());
            while (boundaries.isOpen() && !boundaries.atEnd()) {
                const auto segment = QJsonDocument::fromJson(boundaries.readLine()).object();
                const qint64 a = std::max(slice.start, segment.value("start").toInteger()),
                             b = std::min(slice.end, segment.value("end").toInteger());
                if (b > a)
                    intervals.append({a, b});
                require(intervals.size() <= 100000, QStringLiteral("单个切片的 VAD 边界超过容量限制"));
            }
        }
        std::sort(intervals.begin(), intervals.end());
        QList<QPair<qint64, qint64>> merged;
        for (const auto &interval : intervals) {
            if (!merged.isEmpty() && interval.first <= merged.last().second)
                merged.last().second = std::max(merged.last().second, interval.second);
            else
                merged.append(interval);
        }
        QJsonArray seconds;
        for (const auto &interval : merged)
            seconds.append(
                QJsonArray{(interval.first - slice.start) / 24000.0, (interval.second - slice.start) / 24000.0});
        const QJsonObject row{{"file_name", slice.name}, {"id", "raw_" + slice.name.left(6)}, {"seconds", seconds}};
        const QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
        require(file.write(bytes) == bytes.size(), file.errorString());
    }
    previous.close();
    require(file.commit(), file.errorString());
}

void AudioCache::finish()
{
    if (closed_)
        return;
    closed_ = true;
    guard([this] {
        if (!config_.enabled)
            return;
        while (rawEnd_ < totalSamples_)
            saveRaw(std::min(totalSamples_, rawEnd_ + config_.rawSliceSeconds * 24000LL));
        for (Turn &turn : turns_)
            saveVad(turn);
        rawMetadata();
        require(spool_.flush(), spool_.errorString());
    });
    spool_.close();
    if (config_.enabled && !failed_ && !spool_.fileName().isEmpty() && !spool_.remove()) {
        failed_ = true;
        emit failed(id_, QStringLiteral("无法删除已收尾的 PCM 暂存文件"));
    }
    emit finished(id_);
}
} // namespace aha
