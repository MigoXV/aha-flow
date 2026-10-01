#include "core/settings.h"
#include <QDir>
#include <QStandardPaths>

namespace aha
{
QString AppSettings::defaultCacheDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation))
        .filePath(QStringLiteral("aha-flow/data-bin"));
}

AppSettings AppSettings::load(QSettings &store)
{
    AppSettings s;
    s.engine.baseUrl = store.value("engine/address", s.engine.baseUrl).toString();
    s.engine.model = store.value("engine/model", s.engine.model).toString();
    s.engine.language = store.value("engine/language").toString();
    s.engine.apiKey = store.value("engine/apiKey").toString();
    s.engine.enableAha = store.value("engine/aha", true).toBool();
    s.engine.allowUntrustedCertificate = store.value("engine/allowSelfSigned", true).toBool();
    s.correction.enabled = store.value("correction/enabled", false).toBool();
    s.correction.responsesUrl = store.value("correction/url", s.correction.responsesUrl).toString();
    s.cache.enabled = store.value("cache/enabled", true).toBool();
    s.cache.rootDirectory = store.value("cache/directory", defaultCacheDirectory()).toString();
    s.cache.rawSliceSeconds = qBound(1, store.value("cache/sliceSeconds", 300).toInt(), 3600);
    s.recentServers = store.value("engine/recent").toStringList();
    s.theme = store.value("ui/theme", "vallum").toString() == "abyssus" ? "abyssus" : "vallum";
    s.recentServers.removeDuplicates();
    while (s.recentServers.size() > 5)
        s.recentServers.removeLast();
    return s;
}

void AppSettings::save(QSettings &store) const
{
    store.setValue("engine/address", engine.baseUrl);
    store.setValue("engine/model", engine.model);
    store.setValue("engine/language", engine.language);
    store.setValue("engine/apiKey", engine.apiKey);
    store.setValue("engine/aha", engine.enableAha);
    store.setValue("engine/allowSelfSigned", engine.allowUntrustedCertificate);
    store.setValue("engine/recent", recentServers);
    store.setValue("correction/enabled", correction.enabled);
    store.setValue("correction/url", correction.responsesUrl);
    store.setValue("cache/enabled", cache.enabled);
    store.setValue("cache/directory", cache.rootDirectory);
    store.setValue("cache/sliceSeconds", cache.rawSliceSeconds);
    store.setValue("ui/theme", theme);
    store.sync();
}

QString AppSettings::validate() const
{
    if (engine.modelsUrl().isEmpty())
        return QStringLiteral("请输入有效的 HTTP/HTTPS 服务基地址。");
    if (engine.model.trimmed().isEmpty())
        return QStringLiteral("请选择转写模型。");
    if (cache.enabled &&
        (!QDir::isAbsolutePath(cache.rootDirectory) || cache.rawSliceSeconds < 1 || cache.rawSliceSeconds > 3600))
        return QStringLiteral("缓存目录必须是绝对路径，切片时长应为 1–3600 秒。");
    if (correction.enabled) {
        const QUrl url(correction.responsesUrl.trimmed(), QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment() ||
            (url.scheme() != "http" && url.scheme() != "https"))
            return QStringLiteral("请填写完整的 HTTP/HTTPS 文本纠错 Responses API 地址。");
    }
    return {};
}

void AppSettings::rememberServer()
{
    QUrl url = engine.modelsUrl();
    url.setPath(QString());
    const QString address = url.toString();
    recentServers.removeAll(address);
    recentServers.prepend(address);
    while (recentServers.size() > 5)
        recentServers.removeLast();
}
} // namespace aha
