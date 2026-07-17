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

#include <QTimer>

#include "base/logging.h"
#include "base/codec/audio_decoder.h"
#include "base/threading/worker_manager.h"
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
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioWorker::~AudioWorker()
{
    LOG(INFO) << "Dtor";
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
void AudioWorker::onAudioPacket(std::shared_ptr<proto::audio::Packet> packet)
{
    if (packet)
        decodePacket(*packet);
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
    }
    else
    {
        LOG(ERROR) << "Network worker not found";
    }

    player_ = AudioPlayer::create();
    if (!player_)
        LOG(ERROR) << "Unable to create audio player";

    metrics_timer_ = new QTimer(this);
    metrics_timer_->setInterval(std::chrono::seconds(1));
    connect(metrics_timer_, &QTimer::timeout, this, &AudioWorker::onMetricsTimer);
    metrics_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStop()
{
    LOG(INFO) << "Audio worker stopped";

    delete metrics_timer_;
    metrics_timer_ = nullptr;

    player_.reset();
    decoder_.reset();
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onMetricsTimer()
{
    Metrics metrics;
    metrics.packet_count = packet_count_;
    if (min_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_packet = min_packet_;
    metrics.max_packet = max_packet_;
    metrics.avg_packet = avg_packet_;

    emit sig_metrics(metrics);
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::decodePacket(const proto::audio::Packet& packet)
{
    const size_t packet_size = packet.ByteSizeLong();
    ++packet_count_;
    avg_packet_ = calculateAvgSize(avg_packet_, packet_size);
    min_packet_ = std::min(min_packet_, packet_size);
    max_packet_ = std::max(max_packet_, packet_size);

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
