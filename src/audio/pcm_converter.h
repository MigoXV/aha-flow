#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QList>
struct SRC_STATE_tag;

namespace aha
{
class PcmConverter
{
  public:
    explicit PcmConverter(const QAudioFormat &format);
    ~PcmConverter();
    PcmConverter(const PcmConverter &) = delete;
    PcmConverter &operator=(const PcmConverter &) = delete;
    QList<QByteArray> feed(const QByteArray &data, bool final = false);
    float level() const
    {
        return level_;
    }

  private:
    QAudioFormat format_;
    SRC_STATE_tag *resampler_ = nullptr;
    QByteArray inputTail_;
    QByteArray pcm_;
    float level_ = 0;
};
} // namespace aha
