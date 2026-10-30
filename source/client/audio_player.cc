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

#include "client/audio_player.h"

#include "base/logging.h"
#include "client/audio_output.h"
#include "proto/desktop_audio.h"

namespace {

// A stalled network delivers its audio at once, and the playback would stay behind by that much for
// the rest of the session, so the oldest audio is dropped above this lag.
const size_t kMaxQueuedBytes =
    AudioOutput::kSampleRate * AudioOutput::kChannels * AudioOutput::kBytesPerSample / 2; // 500 ms

} // namespace

//--------------------------------------------------------------------------------------------------
AudioPlayer::AudioPlayer()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioPlayer::~AudioPlayer()
{
    LOG(INFO) << "Dtor";

    // Tear down the output first. Its stop() blocks until any in-flight data callback returns, so
    // |onMoreDataRequired| can no longer touch the queues and mutex below while they are destroyed.
    output_.reset();
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<AudioPlayer> AudioPlayer::create()
{
    std::unique_ptr<AudioPlayer> player(new AudioPlayer());
    if (!player->init())
    {
        LOG(ERROR) << "Unable to initialize audio player";
        return nullptr;
    }

    return player;
}

//--------------------------------------------------------------------------------------------------
void AudioPlayer::addPacket(std::unique_ptr<proto::audio::Packet> packet)
{
    std::scoped_lock lock(incoming_queue_lock_);

    queued_bytes_ += packet->data(0).size();
    incoming_queue_.emplace(std::move(packet));

    while (queued_bytes_ > kMaxQueuedBytes && incoming_queue_.size() > 1)
    {
        queued_bytes_ -= incoming_queue_.front()->data(0).size();
        incoming_queue_.pop();
    }
}

//--------------------------------------------------------------------------------------------------
size_t AudioPlayer::onMoreDataRequired(void* data, size_t size)
{
    if (work_queue_.empty())
    {
        {
            std::scoped_lock lock(incoming_queue_lock_);
            work_queue_.swap(incoming_queue_);
        }

        if (work_queue_.empty())
            return 0;
    }

    size_t target_pos = 0;

    while (!work_queue_.empty())
    {
        const std::string& packet_data = work_queue_.front()->data(0);

        size_t source_size = packet_data.size() - source_pos_;
        size_t target_size = size - target_pos;
        size_t num_bytes = std::min(target_size, source_size);

        memcpy(reinterpret_cast<quint8*>(data) + target_pos,
               packet_data.data() + source_pos_,
               num_bytes);
        target_pos += num_bytes;

        if (target_size < source_size)
        {
            source_pos_ += num_bytes;
            break;
        }
        else
        {
            queued_bytes_ -= packet_data.size();
            work_queue_.pop();
            source_pos_ = 0;

            if (target_size == source_size)
                break;
        }
    }

    return target_pos;
}

//--------------------------------------------------------------------------------------------------
bool AudioPlayer::init()
{
    output_ = AudioOutput::create(std::bind(
        &AudioPlayer::onMoreDataRequired, this, std::placeholders::_1, std::placeholders::_2));
    if (!output_)
    {
        LOG(ERROR) << "AudioOutput::create failed";
        return false;
    }

    if (!output_->start())
    {
        LOG(ERROR) << "AudioOutput::start failed";
        return false;
    }

    return true;
}
