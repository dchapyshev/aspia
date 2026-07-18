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

#include "host/workers/audio_worker.h"

#include "base/logging.h"
#include "base/codec/audio_encoder.h"
#include "base/threading/worker_manager.h"
#include "host/audio_capturer.h"
#include "host/workers/desktop_ipc_worker.h"

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
void AudioWorker::onSetEnabled(bool enable)
{
    if (enable)
    {
        if (encoder_)
            return;

#if !defined(Q_OS_MACOS)
        // Everywhere except macOS a dedicated platform capturer feeds the encoder. On macOS system
        // audio is captured by the screen worker's shared SCStream.
        capturer_ = AudioCapturer::create();
        if (!capturer_)
        {
            LOG(ERROR) << "Unable to create audio capturer";
            return;
        }

        QPointer<AudioWorker> self(this);
        capturer_->start([self](std::unique_ptr<proto::audio::Packet> packet)
        {
            std::shared_ptr<proto::audio::Packet> shared_packet(std::move(packet));
            QMetaObject::invokeMethod(self, [self, shared_packet]()
            {
                if (self)
                    self->encodePacket(*shared_packet);
            }, Qt::QueuedConnection);
        });
#endif // !defined(Q_OS_MACOS)

        LOG(INFO) << "Enabling audio";
        encoder_ = std::make_unique<AudioEncoder>();
    }
    else
    {
        if (!encoder_)
            return;

        LOG(INFO) << "Disabling audio";

        capturer_.reset();
        encoder_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onRawAudioData(std::shared_ptr<proto::audio::Packet> packet)
{
    if (packet)
        encodePacket(*packet);
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStart()
{
    LOG(INFO) << "Audio worker started";

    // All AudioWorker<->DesktopIpcWorker wiring lives here: the IPC worker toggles the pipeline through the
    // merged configuration, and the encoded audio produced here is fanned out through it.
    ipc_worker_ = findWorker<DesktopIpcWorker>();
    if (ipc_worker_)
    {
        connect(ipc_worker_, &DesktopIpcWorker::sig_audioEnabled, this, &AudioWorker::onSetEnabled,
                Qt::QueuedConnection);
        connect(this, &AudioWorker::sig_audioData, ipc_worker_, &DesktopIpcWorker::onAudioData,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "IPC worker not found";
    }

    // Audio is latency-sensitive: a delayed packet is an audible glitch.
    QThread::currentThread()->setPriority(QThread::HighestPriority);
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStop()
{
    LOG(INFO) << "Audio worker stopped";

    if (ipc_worker_)
    {
        ipc_worker_->disconnect(this);
        ipc_worker_ = nullptr;
    }

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
