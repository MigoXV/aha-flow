#pragma once
#include "core/settings.h"
#include "core/transcription.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <functional>
#include <memory>

class QNetworkReply;
namespace aha
{
QString correctionBody(const QString &text, bool final = false);
class SseParser
{
  public:
    using Handler = std::function<void(const QString &, const QJsonObject &)>;
    explicit SseParser(Handler handler) : handler_(std::move(handler)) {}
    void push(const QByteArray &bytes);

  private:
    QByteArray buffer_, data_;
    QString event_;
    Handler handler_;
};
class TextCorrection final : public QObject
{
    Q_OBJECT
  public:
    TextCorrection(CorrectionConfig config, bool allowSelfSigned, QObject *parent = nullptr, int timeoutMs = 60000);
    void enqueue(const Transcript &result);
    void cancel();
  signals:
    void changed(const aha::Transcript &result);

  private:
    void next();
    void finish(const QString &error, const QString &text = {});
    CorrectionConfig config_;
    bool allowSelfSigned_, cancelled_ = false;
    int timeoutMs_;
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    QQueue<Transcript> queue_;
    QSet<QString> seen_;
    QQueue<QString> seenOrder_;
    qsizetype queuedCharacters_ = 0;
    Transcript current_;
    QTimer deadline_;
    std::unique_ptr<SseParser> parser_;
    QString rawText_;
    qint64 responseBytes_ = 0, sequence_ = -1;
};
} // namespace aha
