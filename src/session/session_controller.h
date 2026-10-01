#pragma once
#include "core/settings.h"
#include "core/transcription.h"
#include "storage/audio_cache.h"
#include <QPointer>
#include <QThread>
#include <optional>

namespace aha
{
class SessionWorker;
class SessionController final : public QObject
{
    Q_OBJECT
  public:
    explicit SessionController(QObject *parent = nullptr);
    ~SessionController() override;
    void start(const AppSettings &settings, const QString &replayPcm = {});
    void stop();
    void shutdown();
    void acknowledgeTranscript(const QString &itemId);
    bool active() const
    {
        return !workerStopped_;
    }
  signals:
    void statusChanged(const QString &label, const QString &detail);
    void transcriptChanged(const aha::Transcript &result);
    void levelChanged(float level);
    void connected(const QString &address);
    void started();
    void finished();
    void shutdownReady();
    void cacheDirectory(const QString &path);

  private:
    void launch();
    void settle();
    QThread networkThread_, storageThread_;
    QPointer<SessionWorker> worker_;
    QPointer<AudioCache> cache_;
    std::optional<AppSettings> pending_;
    QString replay_, id_, error_;
    bool workerStopped_ = true, cacheDone_ = true, finishingCache_ = false;
    bool shuttingDown_ = false, stopRequested_ = false;
    bool settled_ = true, shutdownSignalled_ = false;
};
} // namespace aha
