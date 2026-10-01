#include "network/realtime_client.h"
#include "network/proxy.h"
#include "core/transcription.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <stdexcept>

namespace aha
{
RealtimeClient::RealtimeClient(QObject *parent) : QObject(parent)
{
    socket_.setParent(this);
    deadline_.setParent(this);
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { fail(QStringLiteral("连接 Realtime 转写会话超时")); });
    socket_.setMaxAllowedIncomingFrameSize(1024 * 1024);
    socket_.setMaxAllowedIncomingMessageSize(1024 * 1024);
    connect(&socket_, &QWebSocket::textMessageReceived, this, [this](const QString &text) {
        if (stopping_)
            return;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            fail(QStringLiteral("Realtime 返回无效消息"));
            return;
        }
        const QJsonObject event = document.object();
        const QString type = event.value("type").toString();
        emit protocolEvent(event);
        if (stopping_)
            return;
        if (type == "session.created")
            send(sessionUpdate(config_.model, config_.language));
        else if (type == "session.updated" && !ready_) {
            ready_ = true;
            deadline_.stop();
            emit ready();
        } else if (type == "error" || type == "conversation.item.input_audio_transcription.failed")
            fail(event.value("error").toObject().value("message").toString(QStringLiteral("实时转写失败")));
    });
    connect(&socket_, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray &bytes) {
        // Compatible engines may serialize JSON in a binary WebSocket frame.
        QMetaObject::invokeMethod(&socket_, "textMessageReceived", Qt::DirectConnection,
                                  Q_ARG(QString, QString::fromUtf8(bytes)));
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
#else
    connect(&socket_, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError) {
#endif
        if (!stopping_)
            fail(socket_.errorString());
    });
    connect(&socket_, &QWebSocket::disconnected, this, [this] {
        if (!stopping_)
            fail(QStringLiteral("Realtime 连接已断开（%1）").arg(static_cast<int>(socket_.closeCode())));
    });
    connect(&socket_, &QWebSocket::sslErrors, this, [this](const QList<QSslError> &) {
        if (config_.allowUntrustedCertificate)
            socket_.ignoreSslErrors();
    });
}
void RealtimeClient::start(const EngineConfig &config)
{
    config_ = config;
    stopping_ = ready_ = false;
    const QUrl url = config.realtimeUrl();
    if (url.isEmpty()) {
        fail(QStringLiteral("无效的引擎地址"));
        return;
    }
    try {
        socket_.setProxy(engineProxy(url));
    } catch (const std::exception &error) {
        fail(QString::fromUtf8(error.what()));
        return;
    }
    QNetworkRequest request(url);
    if (!config.apiKey.trimmed().isEmpty())
        request.setRawHeader("Authorization", "Bearer " + config.apiKey.trimmed().toUtf8());
    deadline_.start(10000);
    socket_.open(request);
}
void RealtimeClient::send(const QJsonObject &event)
{
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));
}
void RealtimeClient::sendPcm(const QByteArray &pcm)
{
    if (!ready_ || stopping_ || pcm.isEmpty())
        return;
    if (socket_.bytesToWrite() > 5 * 48000 * 4 / 3) {
        fail(QStringLiteral("网络音频积压超过 5 秒"));
        return;
    }
    send({{"type", "input_audio_buffer.append"}, {"audio", QString::fromLatin1(pcm.toBase64())}});
}
void RealtimeClient::stop()
{
    stopping_ = true;
    ready_ = false;
    deadline_.stop();
    socket_.close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("transcription stopped"));
}
void RealtimeClient::fail(const QString &message)
{
    if (stopping_)
        return;
    stop();
    emit failed(message);
}
} // namespace aha
