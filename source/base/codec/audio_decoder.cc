//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <opus.h>

#include "base/logging.h"
#include "base/time_types.h"
#include "proto/desktop_audio.h"

namespace {

// Hosts will never generate more than 100 frames in a single packet.
const int kMaxFramesPerPacket = 100;

const proto::audio::Packet::SamplingRate kSamplingRate = proto::audio::Packet::SAMPLING_RATE_48000;

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
bool AudioDecoder::resetForPacket(const proto::audio::Packet& packet)
{
    if (packet.channels() <= 0 || packet.channels() > 2 || packet.sampling_rate() != kSamplingRate)
    {
        LOG(ERROR) << "Unsupported OPUS parameters:" << packet.channels() << "channels with"
                   << packet.sampling_rate() << "samples per second";
        return false;
    }

    if (packet.channels() != channels_ || packet.sampling_rate() != sampling_rate_)
    {
        destroyDecoder();

        channels_ = packet.channels();
        sampling_rate_ = packet.sampling_rate();
    }

    if (!decoder_)
        initDecoder();

    return decoder_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<proto::audio::Packet> AudioDecoder::decode(const proto::audio::Packet& packet)
{
    if (packet.encoding() != proto::audio::ENCODING_OPUS)
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
    std::unique_ptr<proto::audio::Packet> decoded_packet(new proto::audio::Packet());
    decoded_packet->set_encoding(proto::audio::ENCODING_RAW);
    decoded_packet->set_sampling_rate(kSamplingRate);
    decoded_packet->set_bytes_per_sample(proto::audio::Packet::BYTES_PER_SAMPLE_2);
    decoded_packet->set_channels(packet.channels());

    const int bytes_per_sample = channels_ * decoded_packet->bytes_per_sample();

    // Every frame declares in its TOC byte how many samples it decodes into, so the buffer is
    // sized by the content of the packet instead of the 120 ms worst case per frame.
    int total_samples = 0;

    for (int i = 0; i < packet.data_size(); ++i)
    {
        const std::string& frame = packet.data(i);
        if (frame.empty())
        {
            LOG(ERROR) << "Received an empty OPUS frame";
            return nullptr;
        }

        int frame_samples = opus_packet_get_nb_samples(
            reinterpret_cast<const unsigned char*>(frame.data()),
            static_cast<opus_int32>(frame.size()), kSamplingRate);
        if (frame_samples <= 0)
        {
            LOG(ERROR) << "Invalid OPUS frame. Error code:" << frame_samples;
            return nullptr;
        }

        total_samples += frame_samples;
    }

    std::string* decoded_data = decoded_packet->add_data();
    decoded_data->resize(static_cast<size_t>(total_samples) * static_cast<size_t>(bytes_per_sample));
    int buffer_pos = 0;

    for (int i = 0; i < packet.data_size(); ++i)
    {
        qint16* pcm_buffer = reinterpret_cast<qint16*>(std::data(*decoded_data) + buffer_pos);
        const std::string& frame = packet.data(i);
        const unsigned char* frame_data = reinterpret_cast<const unsigned char*>(frame.data());

        // What is left in the buffer, which is never less than what this frame decodes into.
        int free_samples = static_cast<int>(decoded_data->size() - buffer_pos) / bytes_per_sample;

        int result = opus_decode(decoder_, frame_data, static_cast<opus_int32>(frame.size()),
                                 pcm_buffer, free_samples, 0);
        if (result < 0)
        {
            LOG(ERROR) << "Failed decoding Opus frame. Error code:" << result;
            destroyDecoder();
            return nullptr;
        }

        buffer_pos += result * bytes_per_sample;
    }

    if (!buffer_pos)
        return nullptr;

    decoded_data->resize(static_cast<size_t>(buffer_pos));
    return decoded_packet;
}
