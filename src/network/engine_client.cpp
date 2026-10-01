#include "network/engine_client.h"
#include "network/proxy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QTimer>
#include <stdexcept>

namespace aha
{
namespace
{
constexpr qint64 maxResponseBytes = 1024 * 1024;
}

EngineClient::EngineClient(QObject *parent) : QObject(parent)
{
    network_.setParent(this);
}

void EngineClient::fetchModels(const EngineConfig &config)
{
    if (activeReply_) {
        QNetworkReply *previous = activeReply_;
        activeReply_.clear();
        previous->abort();
    }

    const QUrl url = config.modelsUrl();
    if (url.isEmpty()) {
        emit requestFailed(QStringLiteral("请输入有效的 HTTP/HTTPS 引擎地址。"));
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10000);
    if (!config.apiKey.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + config.apiKey.trimmed().toUtf8());
    }

    try {
        network_.setProxy(engineProxy(url));
    } catch (const std::exception &error) {
        emit requestFailed(QString::fromUtf8(error.what()));
        return;
    }
    QNetworkReply *reply = network_.get(request);
    activeReply_ = reply;
    reply->setReadBufferSize(maxResponseBytes + 1);

    // 总时限独立于传输超时，避免服务器不断发送少量数据而永久占用连接。
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, [reply] {
        reply->setProperty("ahaFailure", QStringLiteral("获取模型列表超时。"));
        reply->abort();
    });
    deadline->start(10000);

    connect(reply, &QNetworkReply::sslErrors, reply, [reply, config](const QList<QSslError> &) {
        if (config.allowUntrustedCertificate) {
            reply->ignoreSslErrors();
        }
    });
    connect(reply, &QIODevice::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > maxResponseBytes) {
            reply->setProperty("ahaFailure", QStringLiteral("模型列表响应超过 1 MiB 限制。"));
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, deadline] {
        deadline->stop();
        reply->deleteLater();
        if (activeReply_ != reply) {
            return;
        }
        activeReply_.clear();

        const QString failure = reply->property("ahaFailure").toString();
        if (!failure.isEmpty()) {
            emit requestFailed(failure);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(QStringLiteral("获取模型列表失败：%1").arg(reply->errorString()));
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status < 200 || status >= 300) {
            emit requestFailed(QStringLiteral("模型列表接口返回 HTTP %1。").arg(status));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !document.object().value(QStringLiteral("data")).isArray()) {
            emit requestFailed(QStringLiteral("模型列表接口返回格式无效。"));
            return;
        }

        QStringList models;
        const QJsonArray data = document.object().value(QStringLiteral("data")).toArray();
        for (const QJsonValue &entry : data) {
            const QString id = entry.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && !models.contains(id)) {
                models.append(id);
            }
        }
        emit modelsReady(models);
    });
}

} // namespace aha
