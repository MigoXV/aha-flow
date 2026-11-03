#include "network/realtime_client.h"
#include "network/proxy.h"
#include "core/transcription.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QRegularExpression>
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
        if ((type == "input_audio_buffer.committed" || type == "input_audio_buffer.speech_started") &&
            !pendingItems_.contains(event.value("item_id").toString()) && pendingItems_.size() >= 1000) {
            fail(QStringLiteral("未完成转写轮次超过容量上限"));
            return;
        }
        emit protocolEvent(event);
        if (stopping_)
            return;
        if (type == "session.created")
            send(requested_);
        else if (type == "session.updated" && !ready_) {
            effective_ = event.value("session").toObject();
            const auto input = effective_.value("audio").toObject().value("input").toObject();
            const auto detection = input.value("turn_detection");
            manualTurns_ = detection.isNull() ||
                           (detection.isUndefined() && (config_.realtimeOptions.value("turn_detection").isNull()));
            ready_ = true;
            emit configurationChanged(requestedConfiguration(), effectiveConfiguration());
            deadline_.stop();
            emit ready();
        } else if (type == "error" || type == "conversation.item.input_audio_transcription.failed") {
            const auto error = event.value("error").toObject();
            QStringList details;
            for (const auto &key : {"type", "code", "param", "message"})
                if (!error.value(key).toString().isEmpty())
                    details.append(QString::fromLatin1(key) + ": " + error.value(key).toString());
            fail(details.isEmpty() ? QStringLiteral("实时转写失败") : details.join('\n'));
        }
        if (type == "input_audio_buffer.committed") {
            if (awaitingCommits_ > 0)
                --awaitingCommits_;
            else if (!manualTurns_)
                uncommittedBytes_ = 0;
            pendingItems_.insert(event.value("item_id").toString());
        }
        if (type == "input_audio_buffer.speech_started")
            pendingItems_.insert(event.value("item_id").toString());
        if (type == "input_audio_buffer.speech_stopped")
            uncommittedBytes_ = 0;
        if (type == "conversation.item.input_audio_transcription.completed" ||
            type == "conversation.item.input_audio_transcription.failed") {
            pendingItems_.remove(event.value("item_id").toString());
            if (!manualTurns_ && pendingItems_.isEmpty())
                uncommittedBytes_ = 0;
        }
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
    requested_ = sessionUpdate(config);
    effective_ = {};
    uncommittedBytes_ = 0;
    awaitingCommits_ = 0;
    pendingItems_.clear();
    manualTurns_ = false;
    stopping_ = ready_ = false;
    emit configurationChanged(requestedConfiguration(), {});
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
    if (pendingItems_.size() + awaitingCommits_ >= 1000) {
        fail(QStringLiteral("未完成转写轮次超过容量上限"));
        return;
    }
    if (socket_.bytesToWrite() > 5 * 48000 * 4 / 3) {
        fail(QStringLiteral("网络音频积压超过 5 秒"));
        return;
    }
    uncommittedBytes_ += pcm.size();
    send({{"type", "input_audio_buffer.append"}, {"audio", QString::fromLatin1(pcm.toBase64())}});
}
bool RealtimeClient::commit()
{
    if (!ready_ || stopping_ || uncommittedBytes_ == 0)
        return false;
    uncommittedBytes_ = 0;
    ++awaitingCommits_;
    send({{"type", "input_audio_buffer.commit"}});
    return true;
}
namespace
{
QJsonValue redactConfiguration(QJsonValue value, const QString &key)
{
    if (value.isObject()) {
        QJsonObject safe;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const auto name = it.key().toLower();
            if (name.contains("secret") || name.contains("token") || name.contains("authorization") ||
                name == "api_key" || name == "apikey")
                continue;
            safe.insert(it.key(), redactConfiguration(it.value(), key));
        }
        return safe;
    }
    if (value.isArray()) {
        QJsonArray safe;
        for (auto entry : value.toArray())
            safe.append(redactConfiguration(entry, key));
        return safe;
    }
    if (value.isString()) {
        QString safe = value.toString();
        if (!key.isEmpty())
            safe.replace(key, "[redacted]");
        safe.replace(QRegularExpression("sk-[A-Za-z0-9_-]+"), "[redacted]");
        return safe;
    }
    return value;
}
} // namespace
QJsonObject RealtimeClient::requestedConfiguration() const
{
    return redactConfiguration(requested_, config_.apiKey).toObject();
}
QJsonObject RealtimeClient::effectiveConfiguration() const
{
    return redactConfiguration(effective_, config_.apiKey).toObject();
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
    QString safe = message;
    if (!config_.apiKey.isEmpty())
        safe.replace(config_.apiKey, "[redacted]");
    emit failed(safe);
}
} // namespace aha
