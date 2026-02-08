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

#ifndef BASE_CODEC_AUDIO_ENCODER_H
#define BASE_CODEC_AUDIO_ENCODER_H

#include "proto/desktop.h"

struct OpusEncoder;

namespace base {

class AudioBus;
class MultiChannelResampler;

class AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    // AudioEncoder interface.
    bool encode(const proto::desktop::AudioPacket& input_packet,
                proto::desktop::AudioPacket* output_packet);
    int bitrate();
    bool setBitrate(int bitrate);

private:
    void initEncoder();
    void destroyEncoder();
    bool resetForPacket(const proto::desktop::AudioPacket& packet);
    void fetchBytesToResample(int resampler_frame_delay, AudioBus* audio_bus);

    int bitrate_ = 96 * 1024; // Output 96 kb/s bitrate.
    int sampling_rate_ = 0;
    proto::desktop::AudioPacket::Channels channels_ = proto::desktop::AudioPacket::CHANNELS_STEREO;
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
    std::unique_ptr<qint16[]> leftover_buffer_;
    int leftover_buffer_size_ = 0;
    int leftover_samples_ = 0;

    Q_DISABLE_COPY(AudioEncoder)
};

} // namespace base

#endif // BASE_CODEC_AUDIO_ENCODER_H
