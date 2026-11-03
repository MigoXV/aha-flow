#pragma once

#include <QString>
#include <QJsonObject>
#include <QUrl>

namespace aha
{

struct EngineConfig {
    QString baseUrl = QStringLiteral("https://192.168.0.222:10000");
    QString model = QStringLiteral("default-audio");
    QString apiKey;
    bool allowUntrustedCertificate = true;
    bool enableAha = true;
    QString language;
    // Wire options and retained editor values are separate: omitted is not null.
    QJsonObject realtimeOptions;
    QJsonObject realtimeDraft;
    QString languageMode = QStringLiteral("single");

    QUrl modelsUrl() const;
    QUrl realtimeUrl() const;

  private:
    QUrl serviceUrl(const QString &path) const;
};

} // namespace aha
