#pragma once
#include "core/engine_config.h"
#include <QJsonObject>
#include <QTimer>
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
  signals:
    void ready();
    void protocolEvent(const QJsonObject &event);
    void failed(const QString &message);

  private:
    void send(const QJsonObject &event);
    void fail(const QString &message);
    QWebSocket socket_;
    QTimer deadline_;
    EngineConfig config_;
    bool ready_ = false, stopping_ = false;
};
} // namespace aha
