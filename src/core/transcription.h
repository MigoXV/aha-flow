#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <optional>
#include <QQueue>
#include <QSet>

namespace aha
{
struct Transcript {
    QString itemId;
    QString text;
    bool final = false;
    bool interim = false;
    bool correcting = false;
    QString correctionError;
};
struct Turn {
    bool listening = true;
    bool semanticPending = false;
    bool speechEnded = false;
    bool suppressInterim = false;
    int revision = 0;
    QString primary;
};
class TranscriptionState
{
  public:
    std::optional<Transcript> apply(const QJsonObject &event);
    QString status() const;
    const QHash<QString, Turn> &turns() const
    {
        return turns_;
    }

  private:
    QHash<QString, Turn> turns_;
    QSet<QString> completed_;
    QQueue<QString> completedOrder_;
};
QJsonObject sessionUpdate(const QString &model, const QString &language);
} // namespace aha
Q_DECLARE_METATYPE(aha::Transcript)
