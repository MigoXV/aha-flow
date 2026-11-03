#pragma once
#include "core/engine_config.h"
#include <QJsonObject>
#include <QTimer>
#include <QSet>
#include <QWebSocket>

namespace aha
{
class RealtimeClient final : public QObject
{
    Q_OBJECT
  public:
    explicit RealtimeClient(QObject *parent = nullptr);
    void start(const EngineConfig &config);
    void sendPcm(const QByteArray &pcm);
    void stop();
    bool commit();
    bool hasPending() const
    {
        return uncommittedBytes_ > 0 || !pendingItems_.isEmpty() || awaitingCommits_ > 0;
    }
    qint64 uncommittedBytes() const
    {
        return uncommittedBytes_;
    }
    bool manualTurns() const
    {
        return manualTurns_;
    }
    QJsonObject requestedConfiguration() const;
    QJsonObject effectiveConfiguration() const;

  signals:
    void ready();
    void configurationChanged(const QJsonObject &requested, const QJsonObject &effective);
    void protocolEvent(const QJsonObject &event);
    void failed(const QString &message);

  private:
    void send(const QJsonObject &event);
    void fail(const QString &message);
    QWebSocket socket_;
    QTimer deadline_;
    EngineConfig config_;
    QJsonObject requested_, effective_;
    QSet<QString> pendingItems_;
    qint64 uncommittedBytes_ = 0;
    int awaitingCommits_ = 0;
    bool manualTurns_ = false;
    bool ready_ = false, stopping_ = false;
};
} // namespace aha
