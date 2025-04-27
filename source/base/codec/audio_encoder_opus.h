//
// Aspia Project
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

#ifndef BASE_CODEC_AUDIO_ENCODER_OPUS_H
#define BASE_CODEC_AUDIO_ENCODER_OPUS_H

#include "base/macros_magic.h"
#include "base/codec/audio_encoder.h"
#include "proto/desktop.h"

struct OpusEncoder;

namespace base {

class AudioBus;
class MultiChannelResampler;

class AudioEncoderOpus final : public AudioEncoder
{
public:
    AudioEncoderOpus();
    ~AudioEncoderOpus() final;

    // AudioEncoder interface.
    bool encode(const proto::AudioPacket& input_packet, proto::AudioPacket* output_packet) final;
    int bitrate() final;
    bool setBitrate(int bitrate) final;

private:
    void initEncoder();
    void destroyEncoder();
    bool resetForPacket(const proto::AudioPacket& packet);
    void fetchBytesToResample(int resampler_frame_delay, AudioBus* audio_bus);

    int bitrate_ = 96 * 1024; // Output 96 kb/s bitrate.
    int sampling_rate_ = 0;
    proto::AudioPacket::Channels channels_ = proto::AudioPacket::CHANNELS_STEREO;
    OpusEncoder* encoder_ = nullptr;

    int frame_size_ = 0;
    std::unique_ptr<MultiChannelResampler> resampler_;
    std::unique_ptr<char[]> resample_buffer_;
    std::unique_ptr<AudioBus> resampler_bus_;

    // Used to pass packet to the FetchBytesToResampler() callback.
    const char* resampling_data_ = nullptr;
    int resampling_data_size_ = 0;
    int resampling_data_pos_ = 0;

    // Left-over unencoded samples from the previous AudioPacket.
    std::unique_ptr<int16_t[]> leftover_buffer_;
    int leftover_buffer_size_ = 0;
    int leftover_samples_ = 0;

    DISALLOW_COPY_AND_ASSIGN(AudioEncoderOpus);
};

} // namespace base

#endif // BASE_CODEC_AUDIO_ENCODER_OPUS_H
