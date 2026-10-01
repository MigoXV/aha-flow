#pragma once

#include "core/engine_config.h"

#include <QNetworkAccessManager>
#include <QPointer>
#include <QStringList>

class QNetworkReply;

namespace aha
{

class EngineClient final : public QObject
{
    Q_OBJECT

  public:
    explicit EngineClient(QObject *parent = nullptr);
    void fetchModels(const EngineConfig &config);

  signals:
    void modelsReady(const QStringList &models);
    void requestFailed(const QString &message);

  private:
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> activeReply_;
};

} // namespace aha
