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

#include "client/workers/audio_worker.h"

#include "base/logging.h"
#include "base/codec/audio_decoder.h"
#include "base/threading/worker.h"
#include "client/audio_player.h"
#include "client/workers/network_worker.h"

namespace {

//--------------------------------------------------------------------------------------------------
size_t calculateAvgSize(size_t last_avg_size, size_t bytes)
{
    static const double kAlpha = 0.1;
    return size_t((kAlpha * double(bytes)) + ((1.0 - kAlpha) * double(last_avg_size)));
}

} // namespace

//--------------------------------------------------------------------------------------------------
AudioWorker::AudioWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioWorker::~AudioWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStart()
{
    LOG(INFO) << "Audio worker started";

    NetworkWorker* network_worker = findWorker<NetworkWorker>();
    if (network_worker)
    {
        // Audio channel (CHANNEL_ID_AUDIO == 7).
        connect(network_worker, &NetworkWorker::sig_channel_7, this, &AudioWorker::onAudioMessage,
                Qt::QueuedConnection);
        // Legacy channel (CHANNEL_ID_LEGACY == 0), where old hosts multiplex audio.
        connect(network_worker, &NetworkWorker::sig_channel_0, this, &AudioWorker::onLegacyMessage,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Network worker not found";
    }

    player_ = AudioPlayer::create();
    if (!player_)
        LOG(ERROR) << "Unable to create audio player";
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStop()
{
    LOG(INFO) << "Audio worker stopped";

    player_.reset();
    decoder_.reset();
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onTimer()
{
    emit sig_metrics(metrics_);
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onAudioMessage(const QByteArray& buffer)
{
    proto::audio::HostToClient* message = incoming_message_.parse<proto::audio::HostToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse audio message";
        return;
    }

    decodePacket(message->packet());
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onLegacyMessage(const QByteArray& buffer)
{
    proto::legacy::SessionToClient* message =
        incoming_message_.parse<proto::legacy::SessionToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse legacy message";
        return;
    }

    if (message->has_audio_packet())
        decodePacket(message->audio_packet());
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::decodePacket(const proto::audio::Packet& packet)
{
    const size_t packet_size = packet.ByteSizeLong();

    // Before the first packet min stays 0; from the first packet on it tracks the real minimum.
    metrics_.min_packet = (metrics_.packet_count == 0) ?
        packet_size : std::min(metrics_.min_packet, packet_size);
    metrics_.max_packet = std::max(metrics_.max_packet, packet_size);
    metrics_.avg_packet = calculateAvgSize(metrics_.avg_packet, packet_size);
    ++metrics_.packet_count;

    if (!player_)
    {
        LOG(ERROR) << "Audio packet received but audio player not initialized";
        return;
    }

    if (packet.encoding() != encoding_)
    {
        if (packet.encoding() != proto::audio::ENCODING_OPUS)
        {
            LOG(WARNING) << "Unsupported audio encoding:" << packet.encoding();
            return;
        }

        decoder_ = std::make_unique<AudioDecoder>();
        encoding_ = packet.encoding();

        LOG(INFO) << "Audio encoding changed to:" << encoding_;
    }

    if (!decoder_)
    {
        LOG(INFO) << "Audio decoder not initialized now";
        return;
    }

    std::unique_ptr<proto::audio::Packet> decoded_packet = decoder_->decode(packet);
    if (decoded_packet)
        player_->addPacket(std::move(decoded_packet));
}
