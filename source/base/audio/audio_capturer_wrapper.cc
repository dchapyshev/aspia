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

#include "base/audio/audio_capturer_wrapper.h"

#include "base/logging.h"
#include "base/audio/audio_capturer.h"

namespace base {

//--------------------------------------------------------------------------------------------------
AudioCapturerWrapper::AudioCapturerWrapper(QObject* parent)
    : QObject(parent),
      thread_(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";

    connect(&thread_, &Thread::started, this, &AudioCapturerWrapper::onBeforeThreadRunning,
            Qt::DirectConnection);
    connect(&thread_, &Thread::finished, this, &AudioCapturerWrapper::onAfterThreadRunning,
            Qt::DirectConnection);
}

//--------------------------------------------------------------------------------------------------
AudioCapturerWrapper::~AudioCapturerWrapper()
{
    LOG(INFO) << "Dtor";
    thread_.stop();
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWrapper::start()
{
    LOG(INFO) << "Starting audio capturer";
    thread_.start();
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWrapper::onBeforeThreadRunning()
{
    thread_.setPriority(Thread::HighestPriority);

    capturer_ = AudioCapturer::create();
    if (!capturer_)
    {
        LOG(ERROR) << "Unable to create audio capturer";
        return;
    }

    capturer_->start([this](std::unique_ptr<proto::desktop::AudioPacket> packet)
    {
        emit sig_audioCaptured(*packet);
    });
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWrapper::onAfterThreadRunning()
{
    capturer_.reset();
}

} // namespace base
