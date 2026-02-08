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

#include "base/codec/audio_decoder.h"

#include "base/logging.h"

#include <opus.h>

namespace base {

namespace {

// Maximum size of an Opus frame in milliseconds.
const std::chrono::milliseconds kMaxFrameSizeMs { 120 };

// Hosts will never generate more than 100 frames in a single packet.
const int kMaxFramesPerPacket = 100;

const proto::desktop::AudioPacket::SamplingRate kSamplingRate =
    proto::desktop::AudioPacket::SAMPLING_RATE_48000;

} // namespace

//--------------------------------------------------------------------------------------------------
AudioDecoder::AudioDecoder() = default;

//--------------------------------------------------------------------------------------------------
AudioDecoder::~AudioDecoder()
{
    destroyDecoder();
}

//--------------------------------------------------------------------------------------------------
void AudioDecoder::initDecoder()
{
    DCHECK(!decoder_);

    int error;
    decoder_ = opus_decoder_create(kSamplingRate, channels_, &error);
    if (!decoder_)
    {
        LOG(ERROR) << "Failed to create OPUS decoder; Error code:" << error;
    }
}

//--------------------------------------------------------------------------------------------------
void AudioDecoder::destroyDecoder()
{
    if (decoder_)
    {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
bool AudioDecoder::resetForPacket(const proto::desktop::AudioPacket& packet)
{
    if (packet.channels() != channels_ || packet.sampling_rate() != sampling_rate_)
    {
        destroyDecoder();

        channels_ = packet.channels();
        sampling_rate_ = packet.sampling_rate();

        if (channels_ <= 0 || channels_ > 2 || sampling_rate_ != kSamplingRate)
        {
            LOG(ERROR) << "Unsupported OPUS parameters:" << channels_ << "channels with"
                       << sampling_rate_ << "samples per second";
            return false;
        }
    }

    if (!decoder_)
        initDecoder();

    return decoder_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<proto::desktop::AudioPacket> AudioDecoder::decode(const proto::desktop::AudioPacket& packet)
{
    if (packet.encoding() != proto::desktop::AUDIO_ENCODING_OPUS)
    {
        LOG(ERROR) << "Received an audio packet with encoding" << packet.encoding()
                   << "when an OPUS packet was expected";
        return nullptr;
    }

    if (packet.data_size() > kMaxFramesPerPacket)
    {
        LOG(ERROR) << "Received an packet with too many frames";
        return nullptr;
    }

    if (!resetForPacket(packet))
        return nullptr;

    // Create a new packet of decoded data.
    std::unique_ptr<proto::desktop::AudioPacket> decoded_packet(new proto::desktop::AudioPacket());
    decoded_packet->set_encoding(proto::desktop::AUDIO_ENCODING_RAW);
    decoded_packet->set_sampling_rate(kSamplingRate);
    decoded_packet->set_bytes_per_sample(proto::desktop::AudioPacket::BYTES_PER_SAMPLE_2);
    decoded_packet->set_channels(packet.channels());

    int max_frame_samples = static_cast<int>(
        kMaxFrameSizeMs * kSamplingRate / std::chrono::milliseconds(1000));
    int max_frame_bytes = max_frame_samples * channels_ * decoded_packet->bytes_per_sample();

    std::string* decoded_data = decoded_packet->add_data();
    decoded_data->resize(
        static_cast<size_t>(packet.data_size()) * static_cast<size_t>(max_frame_bytes));
    int buffer_pos = 0;

    for (int i = 0; i < packet.data_size(); ++i)
    {
        qint16* pcm_buffer = reinterpret_cast<qint16*>(std::data(*decoded_data) + buffer_pos);
        CHECK_LE(buffer_pos + max_frame_bytes, static_cast<int>(decoded_data->size()));
        const std::string& frame = packet.data(i);
        const unsigned char* frame_data = reinterpret_cast<const unsigned char*>(frame.data());
        int result = opus_decode(decoder_, frame_data, static_cast<opus_int32>(frame.size()),
                                 pcm_buffer, max_frame_samples, 0);
        if (result < 0)
        {
            LOG(ERROR) << "Failed decoding Opus frame. Error code:" << result;
            destroyDecoder();
            return nullptr;
        }

        buffer_pos += result * packet.channels() * decoded_packet->bytes_per_sample();
    }

    if (!buffer_pos)
        return nullptr;

    decoded_data->resize(static_cast<size_t>(buffer_pos));
    return decoded_packet;
}

} // namespace base
