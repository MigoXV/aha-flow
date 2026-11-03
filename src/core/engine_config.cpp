#include "core/engine_config.h"

#include <QUrlQuery>
#include <QRegularExpression>

namespace aha
{

QUrl EngineConfig::serviceUrl(const QString &path) const
{
    QString address = baseUrl.trimmed();
    if (address.isEmpty())
        return {};
    if (!address.contains(QStringLiteral("://")))
        address.prepend(QStringLiteral("https://"));
    QUrl url(address, QUrl::StrictMode);
    if (url.scheme() == QStringLiteral("ws"))
        url.setScheme(QStringLiteral("http"));
    else if (url.scheme() == QStringLiteral("wss"))
        url.setScheme(QStringLiteral("https"));
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() ||
        (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        return {};
    }
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

bool EngineConfig::isOfficialOpenAi() const
{
    return modelsUrl().host().compare(QStringLiteral("api.openai.com"), Qt::CaseInsensitive) == 0;
}
QStringList EngineConfig::transcriptionModels(const QStringList &models) const
{
    if (!isOfficialOpenAi())
        return models;
    // Official transcription-session model families, including dated snapshots.
    static const QRegularExpression supported(QStringLiteral(
        "^(whisper-1|gpt-(4o-transcribe(-diarize)?|4o-mini-transcribe|live-transcribe|transcribe|realtime-whisper))"
        "(-[0-9]{4}-[0-9]{2}-[0-9]{2})?$"));
    QStringList result;
    for (const auto &model : models)
        if (supported.match(model).hasMatch() && !result.contains(model))
            result.append(model);
    return result;
}

QUrl EngineConfig::modelsUrl() const
{
    return serviceUrl(QStringLiteral("/v1/models"));
}

QUrl EngineConfig::realtimeUrl() const
{
    QUrl url = serviceUrl(QStringLiteral("/v1/realtime"));
    if (url.isEmpty()) {
        return {};
    }
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("intent"), QStringLiteral("transcription"));
    if (enableAha && url.host().compare(QStringLiteral("api.openai.com"), Qt::CaseInsensitive) != 0) {
        query.addQueryItem(QStringLiteral("x_aha"), QStringLiteral("v1"));
    }
    url.setQuery(query);
    return url;
}

} // namespace aha
