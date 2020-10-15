//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/audio/audio_capturer_wrapper.h"

#include "base/audio/audio_capturer.h"
#include "base/ipc/ipc_channel_proxy.h"

namespace base {

AudioCapturerWrapper::AudioCapturerWrapper(std::shared_ptr<IpcChannelProxy> channel_proxy)
    : channel_proxy_(std::move(channel_proxy)),
      thread_(std::make_unique<Thread>())
{
    // Nothing
}

AudioCapturerWrapper::~AudioCapturerWrapper()
{
    thread_->stop();
}

void AudioCapturerWrapper::start()
{
    thread_->start(MessageLoop::Type::ASIO, this);
}

void AudioCapturerWrapper::onBeforeThreadRunning()
{
    thread_->setPriority(Thread::Priority::HIGHEST);

    capturer_ = AudioCapturer::create();
    capturer_->start([this](std::unique_ptr<proto::AudioPacket> packet)
    {
        outgoing_message_.set_allocated_audio_packet(packet.release());
        channel_proxy_->send(base::serialize(outgoing_message_));
    });
}

void AudioCapturerWrapper::onAfterThreadRunning()
{
    capturer_.reset();
}

} // namespace base
