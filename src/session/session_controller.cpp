#include "session/session_controller.h"
#include "audio/pcm_converter.h"
#include "network/realtime_client.h"
#include "network/text_correction.h"
#include <QAudioSource>
#include <QFile>
#include <QMediaDevices>
#include <QTimer>
#include <QUuid>
#include <memory>

namespace aha
{
class SessionWorker final : public QObject
{
    Q_OBJECT
  public:
    SessionWorker(QString id, AppSettings settings, AudioCache *cache, QString replay)
        : id_(std::move(id)), settings_(std::move(settings)), cache_(cache), replay_(std::move(replay))
    {
    }
    void begin()
    {
        if (stopped_)
            return;
        realtime_ = new RealtimeClient(this);
        correction_ = new TextCorrection(settings_.correction, settings_.engine.allowUntrustedCertificate, this);
        connect(correction_, &TextCorrection::changed, this,
                [this](const Transcript &result) { emit transcript(id_, result); });
        connect(realtime_, &RealtimeClient::failed, this, &SessionWorker::fail);
        connect(realtime_, &RealtimeClient::protocolEvent, this, [this](QJsonObject event) {
            if (stopped_)
                return;
            const QString itemId = event.value("item_id").toString();
            if ((!itemId.isEmpty() && !state_.turns().contains(itemId) && state_.turns().size() >= 1000) ||
                state_.turns().value(itemId).primary.size() + event.value("delta").toString().size() > 512 * 1024) {
                fail(QStringLiteral("转写状态超过容量上限"));
                return;
            }
            auto result = state_.apply(event);
            if (result && result->final && !event.contains("transcript"))
                event.insert("transcript", result->text);
            if (cache_ && !cache_->postEvent(event)) {
                fail(QStringLiteral("缓存事件队列已满"));
                return;
            }
            const QString label = state_.status();
            if (capturingStarted_ &&
                (label != lastLabel_ || event.value("type") == "x_aha.vad.physical_speech_stopped")) {
                lastLabel_ = label;
                emit status(id_, label, {});
            }
            if (result) {
                emit transcript(id_, *result);
                correction_->enqueue(*result);
            }
        });
        connect(realtime_, &RealtimeClient::ready, this, [this] {
            if (stopped_)
                return;
            emit connected(id_);
            try {
                capture();
            } catch (const std::exception &error) {
                fail(QString::fromUtf8(error.what()));
            }
        });
        realtime_->start(settings_.engine);
    }
    void stop(bool cancelCorrection)
    {
        if (cancelCorrection && correction_)
            correction_->cancel();
        if (stopping_)
            return;
        if (stopped_) {
            emit ended(id_);
            return;
        }
        stopping_ = true;
        // Flush the converter before closing the connection, including a short final chunk.
        try {
            if (input_)
                drain();
            if (audio_) {
                disconnect(audio_, nullptr, this, nullptr);
                audio_->stop();
                input_ = nullptr;
            }
            if (converter_)
                for (const QByteArray &pcm : converter_->feed({}, true))
                    push(pcm);
        } catch (const std::exception &error) {
            emit failure(id_, QString::fromUtf8(error.what()));
        }
        stopped_ = true;
        stopping_ = false;
        if (replayTimer_)
            replayTimer_->stop();
        if (levelTimer_)
            levelTimer_->stop();
        if (realtime_)
            realtime_->stop();
        converter_.reset();
        replayFile_.reset();
        emit level(id_, 0);
        emit ended(id_);
    }
  signals:
    void connected(const QString &id);
    void recording(const QString &id);
    void status(const QString &id, const QString &label, const QString &detail);
    void transcript(const QString &id, const aha::Transcript &result);
    void level(const QString &id, float value);
    void failure(const QString &id, const QString &message);
    void ended(const QString &id);

  private:
    void fail(const QString &message)
    {
        if (stopped_ || failing_)
            return;
        failing_ = true;
        emit failure(id_, message);
        stop(false);
    }
    void push(const QByteArray &pcm)
    {
        if (stopped_ || failing_)
            return;
        if (cache_ && !cache_->postPcm(pcm)) {
            fail(QStringLiteral("缓存音频积压超过 5 秒，识别已停止"));
            return;
        }
        realtime_->sendPcm(pcm);
    }
    void drain()
    {
        if (!input_ || stopped_ || failing_)
            return;
        if (input_->bytesAvailable() > audio_->format().bytesForDuration(5000000)) {
            fail(QStringLiteral("麦克风音频积压超过 5 秒"));
            return;
        }
        for (const QByteArray &pcm : converter_->feed(input_->readAll()))
            push(pcm);
    }
    void capture()
    {
        if (!replay_.isEmpty()) {
            replayFile_ = std::make_unique<QFile>(replay_);
            if (!replayFile_->open(QIODevice::ReadOnly) || replayFile_->size() % 2)
                throw std::runtime_error("无法读取 PCM16 回放文件");
            replayTimer_ = new QTimer(this);
            replayTimer_->setInterval(100);
            connect(replayTimer_, &QTimer::timeout, this, [this] {
                if (stopped_)
                    return;
                const QByteArray bytes = replayFile_->read(4800);
                if (bytes.isEmpty()) {
                    replayTimer_->stop();
                    QTimer::singleShot(30000, this, [this] { stop(false); });
                } else
                    push(bytes);
            });
            replayTimer_->start();
        } else {
            const QAudioDevice device = QMediaDevices::defaultAudioInput();
            if (device.isNull())
                throw std::runtime_error("未找到可用麦克风");
            QAudioFormat format;
            format.setSampleRate(24000);
            format.setChannelCount(1);
            format.setSampleFormat(QAudioFormat::Int16);
            if (!device.isFormatSupported(format))
                format = device.preferredFormat();
            converter_ = std::make_unique<PcmConverter>(format);
            audio_ = new QAudioSource(device, format, this);
            audio_->setBufferSize(static_cast<qsizetype>(format.bytesForDuration(200000)));
            connect(audio_, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
                if (!stopped_ && state == QAudio::StoppedState && audio_->error() != QAudio::NoError)
                    fail(QStringLiteral("麦克风采集失败（%1）").arg(static_cast<int>(audio_->error())));
            });
            input_ = audio_->start();
            if (!input_)
                throw std::runtime_error("无法启动麦克风");
            connect(input_, &QIODevice::readyRead, this, [this] {
                try {
                    drain();
                } catch (const std::exception &error) {
                    fail(QString::fromUtf8(error.what()));
                }
            });
            levelTimer_ = new QTimer(this);
            connect(levelTimer_, &QTimer::timeout, this, [this] {
                if (converter_)
                    emit level(id_, converter_->level());
            });
            levelTimer_->start(50);
        }
        emit recording(id_);
        capturingStarted_ = true;
        lastLabel_ = QStringLiteral("等待中");
        emit status(id_, QStringLiteral("等待中"), {});
    }
    QString id_;
    AppSettings settings_;
    QPointer<AudioCache> cache_;
    QString replay_;
    RealtimeClient *realtime_ = nullptr;
    TextCorrection *correction_ = nullptr;
    QAudioSource *audio_ = nullptr;
    QIODevice *input_ = nullptr;
    QTimer *replayTimer_ = nullptr, *levelTimer_ = nullptr;
    std::unique_ptr<PcmConverter> converter_;
    std::unique_ptr<QFile> replayFile_;
    TranscriptionState state_;
    bool stopped_ = false, failing_ = false, stopping_ = false;
    bool capturingStarted_ = false;
    QString lastLabel_;
};

SessionController::SessionController(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<Transcript>();
    networkThread_.setObjectName(QStringLiteral("speech-session"));
    storageThread_.setObjectName(QStringLiteral("audio-cache"));
    networkThread_.start();
    storageThread_.start();
}
SessionController::~SessionController()
{
    networkThread_.quit();
    storageThread_.quit();
    networkThread_.wait();
    storageThread_.wait();
}
void SessionController::start(const AppSettings &settings, const QString &replayPcm)
{
    const QString invalid = settings.validate();
    if (!invalid.isEmpty()) {
        emit statusChanged(QStringLiteral("错误"), invalid);
        return;
    }
    if (shuttingDown_)
        return;
    pending_ = settings;
    replay_ = replayPcm;
    stopRequested_ = true;
    if (worker_) {
        auto worker = worker_;
        QMetaObject::invokeMethod(
            worker,
            [worker] {
                if (worker)
                    worker->stop(true);
            },
            Qt::QueuedConnection);
    }
    if (workerStopped_ && cacheDone_)
        launch();
}
void SessionController::launch()
{
    if (!pending_ || shuttingDown_)
        return;
    if (worker_)
        worker_->deleteLater();
    AppSettings settings = *pending_;
    pending_.reset();
    if (settings.cache.rootDirectory.isEmpty())
        settings.cache.rootDirectory = AppSettings::defaultCacheDirectory();
    id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString id = id_;
    error_.clear();
    workerStopped_ = cacheDone_ = finishingCache_ = stopRequested_ = false;
    settled_ = false;
    cache_ = new AudioCache(id, settings.cache);
    cache_->moveToThread(&storageThread_);
    connect(&storageThread_, &QThread::finished, cache_, &QObject::deleteLater);
    worker_ = new SessionWorker(id, settings, cache_, replay_);
    worker_->moveToThread(&networkThread_);
    connect(&networkThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(cache_, &AudioCache::opened, this, [this, id](const QString &, const QString &directory) {
        if (id != id_)
            return;
        emit cacheDirectory(directory);
        auto worker = worker_;
        if (worker && !stopRequested_)
            QMetaObject::invokeMethod(worker, [worker] {
                if (worker)
                    worker->begin();
            });
    });
    connect(cache_, &AudioCache::failed, this, [this, id](const QString &, const QString &message) {
        if (id != id_)
            return;
        error_ = QStringLiteral("录音缓存失败：%1").arg(message);
        emit statusChanged(QStringLiteral("错误"), error_);
        auto worker = worker_;
        if (worker)
            QMetaObject::invokeMethod(worker, [worker] {
                if (worker)
                    worker->stop(false);
            });
    });
    connect(cache_, &AudioCache::finished, this, [this, id](const QString &) {
        if (id != id_)
            return;
        cache_->deleteLater();
        cache_ = nullptr;
        cacheDone_ = true;
        settle();
    });
    connect(worker_, &SessionWorker::connected, this, [this, settings](const QString &id) {
        if (id == id_ && !stopRequested_) {
            QUrl origin = settings.engine.modelsUrl();
            origin.setPath(QString());
            emit connected(origin.toString());
        }
    });
    connect(worker_, &SessionWorker::recording, this, [this](const QString &id) {
        if (id == id_ && !stopRequested_)
            emit started();
    });
    connect(worker_, &SessionWorker::status, this,
            [this](const QString &id, const QString &label, const QString &detail) {
                if (id == id_ && !stopRequested_ && error_.isEmpty())
                    emit statusChanged(label, detail);
            });
    connect(worker_, &SessionWorker::transcript, this, [this](const QString &id, const Transcript &result) {
        if (id == id_ && !pending_ && !shuttingDown_)
            emit transcriptChanged(result);
    });
    connect(worker_, &SessionWorker::level, this, [this](const QString &id, float value) {
        if (id == id_)
            emit levelChanged(value);
    });
    connect(worker_, &SessionWorker::failure, this, [this](const QString &id, const QString &message) {
        if (id != id_)
            return;
        error_ = message;
        emit statusChanged(QStringLiteral("错误"), message);
    });
    connect(worker_, &SessionWorker::ended, this, [this](const QString &id) {
        if (id != id_)
            return;
        workerStopped_ = true;
        if (cache_ && !finishingCache_) {
            finishingCache_ = true;
            auto cache = cache_;
            QMetaObject::invokeMethod(cache, [cache] {
                if (cache)
                    cache->finish();
            });
        }
        settle();
    });
    emit statusChanged(QStringLiteral("连接中"), {});
    auto cache = cache_;
    QMetaObject::invokeMethod(cache, [cache] {
        if (cache)
            cache->open();
    });
}
void SessionController::stop()
{
    stopRequested_ = true;
    pending_.reset();
    if (!workerStopped_ && error_.isEmpty())
        emit statusChanged(QStringLiteral("停止中"), {});
    auto worker = worker_;
    if (worker)
        QMetaObject::invokeMethod(worker, [worker] {
            if (worker)
                worker->stop(false);
        });
}
void SessionController::acknowledgeTranscript(const QString &itemId)
{
    if (cache_)
        cache_->postEvent({{"stage", "ui-applied"}, {"item_id", itemId}});
}
void SessionController::shutdown()
{
    shuttingDown_ = true;
    pending_.reset();
    stopRequested_ = true;
    auto worker = worker_;
    if (worker)
        QMetaObject::invokeMethod(worker, [worker] {
            if (worker)
                worker->stop(true);
        });
    settle();
}
void SessionController::settle()
{
    if (!workerStopped_ || !cacheDone_)
        return;
    if (shuttingDown_) {
        if (!shutdownSignalled_) {
            shutdownSignalled_ = true;
            emit shutdownReady();
        }
        return;
    }
    if (pending_) {
        launch();
        return;
    }
    if (settled_)
        return;
    settled_ = true;
    emit statusChanged(error_.isEmpty() ? QStringLiteral("待机") : QStringLiteral("错误"), error_);
    emit finished();
}
} // namespace aha
#include "session_controller.moc"
