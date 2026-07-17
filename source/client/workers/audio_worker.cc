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
#include "client/audio_player.h"

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
void AudioWorker::onAudioPacket(std::shared_ptr<proto::audio::Packet> packet)
{
    if (!packet)
        return;

    if (!player_)
    {
        LOG(ERROR) << "Audio packet received but audio player not initialized";
        return;
    }

    if (packet->encoding() != encoding_)
    {
        if (packet->encoding() != proto::audio::ENCODING_OPUS)
        {
            LOG(WARNING) << "Unsupported audio encoding:" << packet->encoding();
            return;
        }

        decoder_ = std::make_unique<AudioDecoder>();
        encoding_ = packet->encoding();

        LOG(INFO) << "Audio encoding changed to:" << encoding_;
    }

    if (!decoder_)
    {
        LOG(INFO) << "Audio decoder not initialized now";
        return;
    }

    std::unique_ptr<proto::audio::Packet> decoded_packet = decoder_->decode(*packet);
    if (decoded_packet)
        player_->addPacket(std::move(decoded_packet));
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStart()
{
    LOG(INFO) << "Audio worker started";

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
