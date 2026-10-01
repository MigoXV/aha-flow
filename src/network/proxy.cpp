#include "network/proxy.h"
#include <QDir>
#include <QFile>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QRegularExpression>
#include <stdexcept>

namespace aha
{
namespace
{
QString proxyVariable(const char *name)
{
    const QByteArray lower = QByteArray(name).toLower();
    if (qEnvironmentVariableIsSet(lower.constData()))
        return qEnvironmentVariable(lower.constData());
    if (qEnvironmentVariableIsSet(name))
        return qEnvironmentVariable(name);
    QFile file(QDir::home().filePath(".bashrc"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QRegularExpression expression(
        QStringLiteral("^\\s*(?:export\\s+)?%1\\s*=\\s*([^\\s]+)\\s*$").arg(QString::fromLatin1(name)));
    for (const QString &line : QString::fromUtf8(file.readAll()).split('\n')) {
        const auto match = expression.match(line);
        if (!match.hasMatch())
            continue;
        QString value = match.captured(1);
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\'')))
            value = value.mid(1, value.size() - 2);
        if (!value.contains('$') && !value.contains('`'))
            return value;
    }
    return {};
}
} // namespace
QNetworkProxy engineProxy(const QUrl &url)
{
    const QString noProxy = qEnvironmentVariable("no_proxy", qEnvironmentVariable("NO_PROXY"));
    for (QString entry : noProxy.split(',', Qt::SkipEmptyParts)) {
        entry = entry.trimmed().toLower();
        if (entry == "*")
            return QNetworkProxy(QNetworkProxy::NoProxy);
        const QUrl rule("http://" + entry);
        if (rule.port() >= 0 && rule.port() != url.port(url.scheme() == "https" || url.scheme() == "wss" ? 443 : 80))
            continue;
        QString host = rule.host();
        if (host.startsWith("*."))
            host.remove(0, 1);
        const QString target = url.host().toLower();
        if (target == host || (!host.isEmpty() && !host.startsWith('.') && target.endsWith('.' + host)) ||
            (host.startsWith('.') && (target.endsWith(host) || target == host.mid(1))))
            return QNetworkProxy(QNetworkProxy::NoProxy);
    }
    const bool secure = url.scheme() == "https" || url.scheme() == "wss";
    const QString configured = proxyVariable(secure ? "HTTPS_PROXY" : "HTTP_PROXY");
    if (!configured.isEmpty()) {
        const QUrl proxy(configured.contains("://") ? configured : "http://" + configured);
        const bool socks = proxy.scheme() == "socks5" || proxy.scheme() == "socks5h";
        if (!proxy.isValid() || proxy.host().isEmpty() || proxy.port() == 0 || (proxy.scheme() != "http" && !socks))
            throw std::runtime_error("代理地址无效；支持 http://、socks5:// 和 socks5h://，不支持到代理的 TLS 连接");
        return QNetworkProxy(socks ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy, proxy.host(),
                             static_cast<quint16>(proxy.port(socks ? 1080 : 8080)), proxy.userName(), proxy.password());
    }
    QUrl http = url;
    if (http.scheme() == "wss")
        http.setScheme("https");
    if (http.scheme() == "ws")
        http.setScheme("http");
    const auto proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(http));
    return proxies.isEmpty() ? QNetworkProxy(QNetworkProxy::NoProxy) : proxies.first();
}
} // namespace aha
