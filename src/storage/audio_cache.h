#pragma once

#include "core/settings.h"
#include <QFile>
#include <QHash>
#include <QJsonObject>
#include <QQueue>
#include <QSet>
#include <QObject>
#include <atomic>
#include <functional>
#include <optional>

namespace aha
{
class AudioCache final : public QObject
{
    Q_OBJECT
  public:
    AudioCache(QString sessionId, CacheConfig config);
    void open();
    // Thread-safe admission bounds queued PCM before posting to the disk thread.
    bool postPcm(const QByteArray &pcm);
    bool postEvent(const QJsonObject &event);
    void finish();
    QString rawDirectory() const
    {
        return rawDirectory_;
    }
  signals:
    void opened(const QString &sessionId, const QString &directory);
    void failed(const QString &sessionId, const QString &message);
    void finished(const QString &sessionId);

  private:
    struct Turn {
        qint64 start = -1;
        qint64 end = -1;
        QString text;
        bool saved = false;
    };
    struct Slice {
        QString name;
        qint64 start;
        qint64 end;
    };
    void guard(const std::function<void()> &operation);
    void append(const QByteArray &pcm);
    void event(const QJsonObject &event);
    void saveRaw(qint64 end);
    void saveVad(Turn &turn);
    void encode(const QString &directory, const QString &name, qint64 start, qint64 end);
    void rawMetadata(std::optional<QPair<qint64, qint64>> boundary = {});
    void appendJson(const QString &path, const QJsonObject &row);
    QString id_;
    CacheConfig config_;
    QString rawDirectory_, vadDirectory_;
    QFile spool_;
    qint64 totalSamples_ = 0, rawEnd_ = 0;
    int rawSequence_ = 0;
    QHash<QString, Turn> turns_;
    QSet<QString> savedItems_;
    QQueue<QString> savedOrder_;
    int vadSequence_ = 0;
    bool failed_ = false, closed_ = false;
    std::atomic<qint64> pendingPcm_{0};
    std::atomic<int> pendingEvents_{0};
};
} // namespace aha
