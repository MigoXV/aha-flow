#include "core/transcription.h"
#include <QJsonArray>
#include <cmath>

namespace aha
{
std::optional<Transcript> TranscriptionState::apply(const QJsonObject &event)
{
    const QString type = event.value("type").toString();
    const QString id = event.value("item_id").toString();
    if (id.isEmpty() || completed_.contains(id))
        return {};
    if (type == "conversation.item.input_audio_transcription.completed") {
        const QString text = event.value("transcript").toString(turns_.value(id).primary);
        turns_.remove(id);
        completed_.insert(id);
        completedOrder_.enqueue(id);
        if (completedOrder_.size() > 1000)
            completed_.remove(completedOrder_.dequeue());
        return Transcript{id, text, true, false, false, {}};
    }
    if (type == "conversation.item.input_audio_transcription.failed") {
        turns_.remove(id);
        completed_.insert(id);
        completedOrder_.enqueue(id);
        if (completedOrder_.size() > 1000)
            completed_.remove(completedOrder_.dequeue());
        return {};
    }
    if (!(type.startsWith("input_audio_buffer.speech_") || type.startsWith("x_aha.") ||
          type == "conversation.item.input_audio_transcription.delta"))
        return {};
    if (!turns_.contains(id) && turns_.size() >= 1000)
        return {};
    Turn &turn = turns_[id];
    if (type == "input_audio_buffer.speech_started")
        turn.listening = !turn.speechEnded;
    if (type == "x_aha.vad.physical_speech_stopped" &&
        (!event.contains("turn_completed") || event.value("turn_completed") == QJsonValue(false)))
        turn.semanticPending = !turn.speechEnded;
    if (type == "x_aha.vad.turn_completed" || type == "input_audio_buffer.speech_stopped") {
        turn.speechEnded = true;
        turn.semanticPending = false;
        turn.listening = false;
    }
    if (type == "x_aha.input_audio_transcription.interim_cleared") {
        turn.suppressInterim = true;
        turn.listening = false;
    }
    if (type == "x_aha.input_audio_transcription.interim" && !turn.suppressInterim) {
        const double revision = event.value("revision").toDouble();
        const QString text = event.value("transcript").toString();
        if (revision > turn.revision && revision <= 2147483647.0 && std::floor(revision) == revision &&
            !text.trimmed().isEmpty()) {
            turn.revision = static_cast<int>(revision);
            return Transcript{id, text, false, true, false, {}};
        }
    }
    if (type == "conversation.item.input_audio_transcription.delta") {
        const QString delta = event.value("delta").toString();
        if (delta.isEmpty())
            return {};
        turn.primary += delta;
        turn.suppressInterim = true;
        turn.listening = false;
        return Transcript{id, turn.primary, false, false, false, {}};
    }
    return {};
}

QString TranscriptionState::status() const
{
    for (const Turn &turn : turns_)
        if (turn.semanticPending)
            return QStringLiteral("语义轮次未结束");
    for (const Turn &turn : turns_)
        if (turn.listening)
            return QStringLiteral("聆听中");
    return turns_.isEmpty() ? QStringLiteral("等待中") : QStringLiteral("转写中");
}

QJsonObject sessionUpdate(const QString &model, const QString &language)
{
    QJsonObject transcription{{"model", model}};
    if (!language.trimmed().isEmpty()) {
        if (model == "gpt-live-transcribe" || model.startsWith("gpt-live-transcribe-"))
            transcription.insert("languages", QJsonArray{language.trimmed()});
        else
            transcription.insert("language", language.trimmed());
    }
    return {
        {"type", "session.update"},
        {"session",
         QJsonObject{{"type", "transcription"},
                     {"audio",
                      QJsonObject{{"input", QJsonObject{{"format", QJsonObject{{"type", "audio/pcm"}, {"rate", 24000}}},
                                                        {"transcription", transcription}}}}}}}};
}
} // namespace aha
