//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/audio/audio_capturer_linux.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_task_runner.h"

namespace base {

//--------------------------------------------------------------------------------------------------
AudioCapturerLinux::AudioCapturerLinux()
    : silence_detector_(0)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioCapturerLinux::~AudioCapturerLinux()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerLinux::start(const PacketCapturedCallback& callback)
{
    callback_ = callback;
    DCHECK(callback_);

    std::filesystem::path pipe_path("/etc/pulse/fifo_output"); // Correct way?
    if (pipe_path.empty())
    {
        LOG(LS_ERROR) << "Empty pipe path";
        return false;
    }

    pipe_reader_ = AudioPipeReader::create(
        MessageLoopTaskRunner::current(), pipe_path, this);
    if (!pipe_reader_)
    {
        LOG(LS_ERROR) << "AudioPipeReader::create failed";
        return false;
    }

    silence_detector_.reset(AudioPipeReader::kSamplingRate, AudioPipeReader::kChannels);

    pipe_reader_->start();
    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::onDataRead(const std::string& data)
{
    DCHECK(callback_);

    if (silence_detector_.isSilence(reinterpret_cast<const int16_t*>(data.data()),
                                    data.size() / sizeof(int16_t) / AudioPipeReader::kChannels))
    {
        return;
    }

    std::unique_ptr<proto::AudioPacket> packet = std::make_unique<proto::AudioPacket>();
    packet->add_data(data);
    packet->set_encoding(proto::AUDIO_ENCODING_RAW);
    packet->set_sampling_rate(AudioPipeReader::kSamplingRate);
    packet->set_bytes_per_sample(AudioPipeReader::kBytesPerSample);
    packet->set_channels(AudioPipeReader::kChannels);

    callback_(std::move(packet));
}

} // namespace base
