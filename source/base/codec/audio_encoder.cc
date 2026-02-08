//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/codec/audio_encoder.h"

#include "base/logging.h"
#include "base/codec/audio_bus.h"
#include "base/codec/audio_sample_types.h"
#include "base/codec/multi_channel_resampler.h"

#include <opus.h>

namespace base {

namespace {

// Opus doesn't support 44100 sampling rate so we always resample to 48kHz.
const proto::desktop::AudioPacket::SamplingRate kOpusSamplingRate =
    proto::desktop::AudioPacket::SAMPLING_RATE_48000;

// Opus supports frame sizes of 2.5, 5, 10, 20, 40 and 60 ms. We use 20 ms
// frames to balance latency and efficiency.
const std::chrono::milliseconds kFrameSizeMs { 20 };

// Number of samples per frame when using default sampling rate.
const int kFrameSamples = static_cast<const int>(
    kOpusSamplingRate * kFrameSizeMs / std::chrono::milliseconds(1000));

const proto::desktop::AudioPacket::BytesPerSample kBytesPerSample =
    proto::desktop::AudioPacket::BYTES_PER_SAMPLE_2;

//--------------------------------------------------------------------------------------------------
bool isSupportedSampleRate(int rate)
{
    return rate == 44100 || rate == 48000 || rate == 96000 || rate == 192000;
}

} // namespace

//--------------------------------------------------------------------------------------------------
AudioEncoder::AudioEncoder() = default;

//--------------------------------------------------------------------------------------------------
AudioEncoder::~AudioEncoder()
{
    destroyEncoder();
}

//--------------------------------------------------------------------------------------------------
void AudioEncoder::initEncoder()
{
    DCHECK(!encoder_);

    int error;
    encoder_ = opus_encoder_create(kOpusSamplingRate, channels_, OPUS_APPLICATION_AUDIO, &error);
    if (!encoder_)
    {
        LOG(ERROR) << "Failed to create OPUS encoder. Error code:" << error;
        return;
    }

    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate_));

    frame_size_ = int(sampling_rate_ * kFrameSizeMs / std::chrono::milliseconds(1000));

    if (sampling_rate_ != kOpusSamplingRate)
    {
        resample_buffer_.reset(new char[kFrameSamples * kBytesPerSample * size_t(channels_)]);
        // TODO(sergeyu): Figure out the right buffer size to use per packet instead
        // of using SincResampler::kDefaultRequestSize.
        resampler_.reset(new MultiChannelResampler(
            channels_,
            double(sampling_rate_) / double(kOpusSamplingRate),
            SincResampler::kDefaultRequestSize,
            std::bind(&AudioEncoder::fetchBytesToResample,
                this, std::placeholders::_1, std::placeholders::_2)));
        resampler_bus_ = AudioBus::Create(channels_, kFrameSamples);
    }

    // Drop leftover data because it's for different sampling rate.
    leftover_samples_ = 0;
    leftover_buffer_size_ = frame_size_ + SincResampler::kDefaultRequestSize;
    leftover_buffer_.reset(
        new qint16[size_t(leftover_buffer_size_) * size_t(channels_)]);
}

//--------------------------------------------------------------------------------------------------
void AudioEncoder::destroyEncoder()
{
    if (encoder_)
    {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }

    resampler_.reset();
}

//--------------------------------------------------------------------------------------------------
bool AudioEncoder::resetForPacket(const proto::desktop::AudioPacket& packet)
{
    if (packet.channels() != channels_ || packet.sampling_rate() != sampling_rate_)
    {
        destroyEncoder();

        channels_ = packet.channels();
        sampling_rate_ = packet.sampling_rate();

        if (channels_ <= 0 || channels_ > 2 || !isSupportedSampleRate(sampling_rate_))
        {
            LOG(ERROR) << "Unsupported OPUS parameters:" << channels_ << "channels with"
                       << sampling_rate_ << "samples per second";
            return false;
        }

        initEncoder();
    }

    return encoder_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
void AudioEncoder::fetchBytesToResample(int /* resampler_frame_delay */, AudioBus* audio_bus)
{
    DCHECK(resampling_data_);

    int samples_left = (resampling_data_size_ - resampling_data_pos_) / kBytesPerSample / channels_;
    DCHECK_LE(audio_bus->frames(), samples_left);

    audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(
        reinterpret_cast<const qint16*>(resampling_data_ + resampling_data_pos_),
        audio_bus->frames());

    resampling_data_pos_ += audio_bus->frames() * kBytesPerSample * channels_;
    DCHECK_LE(resampling_data_pos_, int(resampling_data_size_));
}

//--------------------------------------------------------------------------------------------------
int AudioEncoder::bitrate()
{
    return bitrate_;
}

//--------------------------------------------------------------------------------------------------
bool AudioEncoder::setBitrate(int bitrate)
{
    if (!encoder_)
        return false;

    switch (bitrate)
    {
        case 128 * 1024:
        case 96 * 1024:
        case 64 * 1024:
        case 32 * 1024:
        case 24 * 1024:
        case 10 * 1024:
            break;

        default:
            LOG(ERROR) << "Invalid bitrate value:" << bitrate;
            return false;
    }

    LOG(INFO) << "Bitrate changed from" << bitrate_ << "to" << bitrate;
    bitrate_ = bitrate;

    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate));
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AudioEncoder::encode(
    const proto::desktop::AudioPacket& input_packet, proto::desktop::AudioPacket* output_packet)
{
    DCHECK_EQ(proto::desktop::AUDIO_ENCODING_RAW, input_packet.encoding());
    DCHECK_EQ(1, input_packet.data_size());
    DCHECK_EQ(kBytesPerSample, input_packet.bytes_per_sample());

    if (!resetForPacket(input_packet))
    {
        LOG(ERROR) << "Encoder initialization failed";
        return false;
    }

    int samples_in_packet =
        static_cast<int>(input_packet.data(0).size() / kBytesPerSample / quint32(channels_));
    const qint16* next_sample = reinterpret_cast<const qint16*>(input_packet.data(0).data());

    // Create a new packet of encoded data.
    output_packet->set_encoding(proto::desktop::AUDIO_ENCODING_OPUS);
    output_packet->set_sampling_rate(kOpusSamplingRate);
    output_packet->set_channels(channels_);

    int prefetch_samples = resampler_.get() ? SincResampler::kDefaultRequestSize : 0;
    int samples_wanted = frame_size_ + prefetch_samples;

    while (leftover_samples_ + samples_in_packet >= samples_wanted)
    {
        const qint16* pcm_buffer = nullptr;

        // Combine the packet with the leftover samples, if any.
        if (leftover_samples_ > 0)
        {
            pcm_buffer = leftover_buffer_.get();
            int samples_to_copy = samples_wanted - leftover_samples_;
            memcpy(leftover_buffer_.get() + leftover_samples_ * channels_,
                   next_sample,
                   size_t(samples_to_copy) * kBytesPerSample * size_t(channels_));
        }
        else
        {
            pcm_buffer = next_sample;
        }

        // Resample data if necessary.
        int samples_consumed = 0;
        if (resampler_.get())
        {
            resampling_data_ = reinterpret_cast<const char*>(pcm_buffer);
            resampling_data_pos_ = 0;
            resampling_data_size_ = samples_wanted * channels_ * kBytesPerSample;
            resampler_->Resample(kFrameSamples, resampler_bus_.get());
            resampling_data_ = nullptr;
            samples_consumed = resampling_data_pos_ / channels_ / kBytesPerSample;

            resampler_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
                kFrameSamples, reinterpret_cast<qint16*>(resample_buffer_.get()));
            pcm_buffer = reinterpret_cast<qint16*>(resample_buffer_.get());
        }
        else
        {
            samples_consumed = frame_size_;
        }

        // Initialize output buffer.
        std::string* data = output_packet->add_data();
        data->resize(kFrameSamples * kBytesPerSample * size_t(channels_));

        // Encode.
        unsigned char* buffer = reinterpret_cast<unsigned char*>(std::data(*data));
        int result = opus_encode(encoder_, pcm_buffer, kFrameSamples, buffer,
                                 opus_int32(data->length()));
        if (result < 0)
        {
            LOG(ERROR) << "opus_encode() failed with error code:" << result;
            return false;
        }

        DCHECK_LE(result, int(data->length()));
        data->resize(size_t(result));

        // Cleanup leftover buffer.
        if (samples_consumed >= leftover_samples_)
        {
            samples_consumed -= leftover_samples_;
            leftover_samples_ = 0;
            next_sample += samples_consumed * channels_;
            samples_in_packet -= samples_consumed;
        }
        else
        {
            leftover_samples_ -= samples_consumed;
            memmove(leftover_buffer_.get(),
                    leftover_buffer_.get() + samples_consumed * channels_,
                    size_t(leftover_samples_) * size_t(channels_) * kBytesPerSample);
        }
    }

    // Store the leftover samples.
    if (samples_in_packet > 0)
    {
        DCHECK_LE(leftover_samples_ + samples_in_packet, leftover_buffer_size_);
        memmove(leftover_buffer_.get() + leftover_samples_ * channels_,
                next_sample,
                size_t(samples_in_packet) * kBytesPerSample * size_t(channels_));
        leftover_samples_ += samples_in_packet;
    }

    // Return nullptr if there's nothing in the packet.
    if (output_packet->data_size() == 0)
        return false;

    return true;
}

} // namespace base
