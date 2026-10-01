#pragma once

#include "core/engine_config.h"
#include <QSettings>

namespace aha
{
struct CacheConfig {
    bool enabled = true;
    QString rootDirectory;
    int rawSliceSeconds = 300;
};
struct CorrectionConfig {
    bool enabled = false;
    QString responsesUrl = QStringLiteral("http://192.168.0.222:10002/v1/responses");
};
struct AppSettings {
    EngineConfig engine;
    CacheConfig cache;
    CorrectionConfig correction;
    QStringList recentServers;
    QString theme = QStringLiteral("vallum");

    static QString defaultCacheDirectory();
    static AppSettings load(QSettings &store);
    void save(QSettings &store) const;
    QString validate() const;
    void rememberServer();
};
} // namespace aha
Q_DECLARE_METATYPE(aha::AppSettings)
