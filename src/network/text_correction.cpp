#include "network/text_correction.h"
#include "network/proxy.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <stdexcept>

namespace aha
{
QString correctionBody(const QString &text, bool final)
{
    const QString marker = QStringLiteral("<KEY>");
    const qsizetype index = text.indexOf(marker);
    if (index >= 0)
        return text.left(index).trimmed();
    if (!final)
        for (int n = 4; n > 0; --n)
            if (text.endsWith(marker.left(n)))
                return text.left(text.size() - n).trimmed();
    return text.trimmed();
}
void SseParser::push(const QByteArray &bytes)
{
    buffer_.append(bytes);
    while (true) {
        int index = -1;
        for (int i = 0; i < buffer_.size(); ++i)
            if (buffer_[i] == '\r' || buffer_[i] == '\n') {
                index = i;
                break;
            }
        if (index < 0 || (buffer_[index] == '\r' && index == buffer_.size() - 1))
            return;
        const QByteArray line = buffer_.left(index);
        const int separator = buffer_.mid(index, 2) == "\r\n" ? 2 : 1;
        buffer_.remove(0, index + separator);
        if (line.isEmpty()) {
            if (!data_.isEmpty()) {
                QJsonParseError error;
                const QJsonDocument document = QJsonDocument::fromJson(data_, &error);
                if (error.error != QJsonParseError::NoError || !document.isObject())
                    throw std::runtime_error("纠错服务返回无效 SSE 数据");
                const QString event = event_;
                data_.clear();
                event_.clear();
                handler_(event, document.object());
            } else
                event_.clear();
        } else if (!line.startsWith(':')) {
            const int colon = line.indexOf(':');
            const QByteArray field = colon < 0 ? line : line.left(colon);
            QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
            if (value.startsWith(' '))
                value.remove(0, 1);
            if (field == "event")
                event_ = QString::fromUtf8(value);
            if (field == "data") {
                if (!data_.isEmpty())
                    data_.append('\n');
                data_.append(value);
            }
        }
    }
}
TextCorrection::TextCorrection(CorrectionConfig config, bool allowSelfSigned, QObject *parent, int timeoutMs)
    : QObject(parent), config_(std::move(config)), allowSelfSigned_(allowSelfSigned), timeoutMs_(timeoutMs)
{
    network_.setParent(this);
    deadline_.setParent(this);
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { finish(QStringLiteral("文本纠错超时")); });
}
void TextCorrection::enqueue(const Transcript &result)
{
    if (!config_.enabled || cancelled_ || !result.final || result.text.trimmed().isEmpty() ||
        seen_.contains(result.itemId))
        return;
    if (queue_.size() >= 1000 || queuedCharacters_ + result.text.size() > 512 * 1024) {
        Transcript failed = result;
        failed.correctionError = QStringLiteral("纠错队列已满，保留原文");
        emit changed(failed);
        return;
    }
    seen_.insert(result.itemId);
    seenOrder_.enqueue(result.itemId);
    if (seenOrder_.size() > 1000)
        seen_.remove(seenOrder_.dequeue());
    queuedCharacters_ += result.text.size();
    queue_.enqueue(result);
    if (!reply_)
        next();
}
void TextCorrection::cancel()
{
    cancelled_ = true;
    queue_.clear();
    queuedCharacters_ = 0;
    deadline_.stop();
    if (reply_) {
        auto *reply = reply_.data();
        reply_.clear();
        reply->abort();
        reply->deleteLater();
    }
}
void TextCorrection::next()
{
    if (cancelled_ || reply_ || queue_.isEmpty())
        return;
    current_ = queue_.dequeue();
    queuedCharacters_ -= current_.text.size();
    rawText_.clear();
    responseBytes_ = 0;
    sequence_ = -1;
    parser_ = std::make_unique<SseParser>([this](const QString &name, const QJsonObject &event) {
        if (!reply_)
            return;
        if (event.contains("sequence_number")) {
            const double number = event.value("sequence_number").toDouble(-1);
            if (number < 0 || number > 1e12 || number != static_cast<qint64>(number) || number <= sequence_)
                throw std::runtime_error("纠错事件顺序无效");
            sequence_ = static_cast<qint64>(number);
        }
        const QString type = event.value("type").toString(name);
        if (type == "response.output_text.delta") {
            if (!event.value("delta").isString())
                throw std::runtime_error("无效的纠错文本增量");
            rawText_ += event.value("delta").toString();
            Transcript update = current_;
            update.text = correctionBody(rawText_);
            update.correcting = true;
            emit changed(update);
        } else if (type == "response.completed") {
            const QJsonObject response = event.value("response").toObject();
            if (response.value("status").toString() != "completed" || !response.value("output").isArray())
                throw std::runtime_error("纠错最终响应无效");
            QString text;
            for (const QJsonValue &itemValue : response.value("output").toArray()) {
                const QJsonObject item = itemValue.toObject();
                if (item.value("type") != "message" || item.value("role") != "assistant")
                    continue;
                for (const QJsonValue &part : item.value("content").toArray())
                    if (part.toObject().value("type") == "output_text")
                        text += part.toObject().value("text").toString();
            }
            text = correctionBody(text, true);
            if (text.isEmpty())
                throw std::runtime_error("纠错结果为空");
            finish({}, text);
        } else if (type == "response.incomplete")
            throw std::runtime_error("纠错输出被截断");
        else if (type == "response.failed" || type == "error")
            throw std::runtime_error("纠错服务推理失败");
    });
    const QUrl url(config_.responsesUrl);
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(timeoutMs_);
    try {
        network_.setProxy(engineProxy(url));
    } catch (const std::exception &error) {
        Transcript update = current_;
        update.correctionError = QString::fromUtf8(error.what());
        emit changed(update);
        QTimer::singleShot(0, this, &TextCorrection::next);
        return;
    }
    QNetworkReply *reply = network_.post(request, QJsonDocument(QJsonObject{{"model", "AgenticASR-Refiner"},
                                                                            {"input", current_.text},
                                                                            {"temperature", 0},
                                                                            {"stream", true},
                                                                            {"store", false}})
                                                      .toJson(QJsonDocument::Compact));
    reply_ = reply;
    connect(reply, &QNetworkReply::sslErrors, reply, [this, reply](const QList<QSslError> &) {
        if (allowSelfSigned_)
            reply->ignoreSslErrors();
    });
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (reply_ != reply)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status < 200 || status >= 300) {
            finish(QStringLiteral("纠错接口返回 HTTP %1").arg(status));
            return;
        }
        const QByteArray bytes = reply->readAll();
        responseBytes_ += bytes.size();
        if (responseBytes_ > 1024 * 1024) {
            finish(QStringLiteral("纠错响应超过 1 MiB"));
            return;
        }
        try {
            parser_->push(bytes);
        } catch (const std::exception &error) {
            finish(QString::fromUtf8(error.what()));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply_ == reply)
            finish(reply->error() == QNetworkReply::NoError ? QStringLiteral("纠错流在最终事件前断开")
                                                            : reply->errorString());
    });
    deadline_.start(timeoutMs_);
    Transcript update = current_;
    update.text.clear();
    update.correcting = true;
    emit changed(update);
}
void TextCorrection::finish(const QString &error, const QString &text)
{
    if (!reply_)
        return;
    QNetworkReply *reply = reply_;
    reply_.clear();
    deadline_.stop();
    reply->abort();
    reply->deleteLater();
    Transcript update = current_;
    update.text = error.isEmpty() ? text : current_.text;
    update.correcting = false;
    update.correctionError = error;
    emit changed(update);
    // Defer: parser_->push may still be on the stack when final completion arrives.
    QTimer::singleShot(0, this, &TextCorrection::next);
}
} // namespace aha
