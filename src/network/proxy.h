#pragma once
#include <QNetworkProxy>
#include <QUrl>
namespace aha
{
QNetworkProxy engineProxy(const QUrl &url);
}
