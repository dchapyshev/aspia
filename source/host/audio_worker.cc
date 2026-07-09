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

#include "host/audio_worker.h"

#include "base/logging.h"
#include "base/audio/audio_capturer.h"
#include "base/codec/audio_encoder.h"

//--------------------------------------------------------------------------------------------------
// On macOS the Qt dispatcher backs the thread with a CFRunLoop (via
// QT_EVENT_DISPATCHER_CORE_FOUNDATION); everywhere else asio is used.
AudioWorker::AudioWorker()
#if defined(Q_OS_MACOS)
    : Worker(Thread::QtDispatcher)
#else
    : Worker(Thread::AsioDispatcher)
#endif
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioWorker::~AudioWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::setEnabled(bool enable)
{
    if (enable)
    {
        if (capturer_)
            return;

        LOG(INFO) << "Starting audio capturer";

        capturer_ = AudioCapturer::create();
        if (!capturer_)
        {
            LOG(ERROR) << "Unable to create audio capturer";
            return;
        }

        encoder_ = std::make_unique<AudioEncoder>();

        // The platform capturer delivers packets on its own thread; marshal each one to the worker
        // thread for encoding. The shared_ptr keeps the packet alive inside the copyable functor
        // without copying the PCM data itself.
        capturer_->start([this](std::unique_ptr<proto::audio::Packet> packet)
        {
            std::shared_ptr<proto::audio::Packet> shared_packet(std::move(packet));
            QMetaObject::invokeMethod(this, [this, shared_packet]()
            {
                encodePacket(*shared_packet);
            }, Qt::QueuedConnection);
        });
    }
    else
    {
        if (!capturer_)
            return;

        LOG(INFO) << "Stopping audio capturer";

        capturer_.reset();
        encoder_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStart()
{
    LOG(INFO) << "Audio worker started";

    // Audio is latency-sensitive: a delayed packet is an audible glitch.
    QThread::currentThread()->setPriority(QThread::HighestPriority);
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStop()
{
    LOG(INFO) << "Audio worker stopped";

    capturer_.reset();
    encoder_.reset();
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::encodePacket(const proto::audio::Packet& packet)
{
    // The capturer may already be stopped while marshalled packets are still queued.
    if (!encoder_)
        return;

    if (!encoder_->encode(packet, serializer_.newMessage<proto::audio::HostToClient>().mutable_packet()))
        return;

    emit sig_audioData(serializer_.serialize<proto::audio::HostToClient>());
}
