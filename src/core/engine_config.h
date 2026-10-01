#pragma once

#include <QString>
#include <QUrl>

namespace aha {

struct EngineConfig {
    QString baseUrl = QStringLiteral("https://192.168.0.222:10000");
    QString model = QStringLiteral("default-audio");
    QString apiKey;
    bool allowUntrustedCertificate = false;
    bool enableAha = true;

    QUrl modelsUrl() const;
    QUrl realtimeUrl() const;

private:
    QUrl serviceUrl(const QString &path) const;
};

} // namespace aha
