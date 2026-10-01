#include "audio/pcm_converter.h"
#include <samplerate.h>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace aha
{
PcmConverter::PcmConverter(const QAudioFormat &format) : format_(format)
{
    if (!format.isValid())
        throw std::runtime_error("无效的麦克风格式");
    int error = 0;
    resampler_ = src_new(SRC_SINC_FASTEST, 1, &error);
    if (!resampler_)
        throw std::runtime_error(src_strerror(error));
}
PcmConverter::~PcmConverter()
{
    src_delete(resampler_);
}

QList<QByteArray> PcmConverter::feed(const QByteArray &data, bool final)
{
    inputTail_.append(data);
    if (inputTail_.size() > format_.bytesForDuration(5000000))
        throw std::runtime_error("麦克风音频积压超过 5 秒");
    const int frameBytes = format_.bytesPerFrame();
    // libsamplerate flushes its filter tail only when the final call carries input.
    // Keep one complete frame until the next block or the explicit final call.
    const int availableFrames = static_cast<int>(inputTail_.size() / frameBytes);
    const int frames = final ? availableFrames : qMax(0, availableFrames - 1);
    std::vector<float> mono(static_cast<size_t>(frames));
    double energy = 0;
    for (int frame = 0; frame < frames; ++frame) {
        float sum = 0;
        for (int channel = 0; channel < format_.channelCount(); ++channel) {
            const char *p = inputTail_.constData() + frame * frameBytes + channel * format_.bytesPerSample();
            float sample = 0;
            switch (format_.sampleFormat()) {
            case QAudioFormat::UInt8:
                sample = (static_cast<unsigned char>(*p) - 128) / 128.0f;
                break;
            case QAudioFormat::Int16: {
                qint16 value;
                std::memcpy(&value, p, 2);
                sample = value / 32768.0f;
                break;
            }
            case QAudioFormat::Int32: {
                qint32 value;
                std::memcpy(&value, p, 4);
                sample = static_cast<float>(value / 2147483648.0);
                break;
            }
            case QAudioFormat::Float:
                std::memcpy(&sample, p, 4);
                break;
            default:
                throw std::runtime_error("不支持的麦克风采样格式");
            }
            if (!std::isfinite(sample))
                sample = 0;
            sum += sample;
        }
        mono[static_cast<size_t>(frame)] = sum / format_.channelCount();
        energy += mono[static_cast<size_t>(frame)] * mono[static_cast<size_t>(frame)];
    }
    if (frames)
        level_ = static_cast<float>(std::min(1.0, std::sqrt(energy / frames) * 8));
    inputTail_.remove(0, frames * frameBytes);
    if (final && !inputTail_.isEmpty())
        throw std::runtime_error("麦克风音频尾帧不完整");

    const double ratio = 24000.0 / format_.sampleRate();
    std::vector<float> output(static_cast<size_t>(std::ceil(frames * ratio)) + 512);
    long offset = 0;
    bool again = true;
    while (again) {
        SRC_DATA conversion{};
        conversion.data_in = frames ? mono.data() + offset : nullptr;
        conversion.input_frames = frames - offset;
        conversion.data_out = output.data();
        conversion.output_frames = static_cast<long>(output.size());
        conversion.src_ratio = ratio;
        conversion.end_of_input = final ? 1 : 0;
        const int error = src_process(resampler_, &conversion);
        if (error)
            throw std::runtime_error(src_strerror(error));
        offset += conversion.input_frames_used;
        for (long i = 0; i < conversion.output_frames_gen; ++i) {
            const float sample = std::clamp(output[static_cast<size_t>(i)], -1.0f, 1.0f);
            const auto value = static_cast<qint16>(
                std::clamp<long>(std::lround(sample * (sample < 0 ? 32768 : 32767)), -32768, 32767));
            char bytes[2];
            qToLittleEndian<qint16>(value, bytes);
            pcm_.append(bytes, 2);
        }
        again = offset < frames || (final && conversion.output_frames_gen > 0);
        if (!conversion.input_frames_used && !conversion.output_frames_gen)
            break;
    }
    QList<QByteArray> chunks;
    while (pcm_.size() >= 4800) {
        chunks.append(pcm_.left(4800));
        pcm_.remove(0, 4800);
    }
    if (final && !pcm_.isEmpty()) {
        chunks.append(pcm_);
        pcm_.clear();
    }
    return chunks;
}
} // namespace aha
