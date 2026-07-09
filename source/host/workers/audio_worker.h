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

#ifndef HOST_WORKERS_AUDIO_WORKER_H
#define HOST_WORKERS_AUDIO_WORKER_H

#include <memory>

#include <QPointer>

#include "base/serialization.h"
#include "base/threading/worker.h"
#include "proto/desktop_audio.h"

class AudioCapturer;
class AudioEncoder;
class IpcWorker;

class AudioWorker final : public Worker
{
    Q_OBJECT

public:
    AudioWorker();
    ~AudioWorker() final;

public slots:
    // Starts or stops capturing and encoding. Runs in the worker thread; invoke it through a
    // queued connection.
    void onSetEnabled(bool enable);

signals:
    // Serialized audio message with one encoded packet, ready to be sent to clients.
    void sig_audioData(const QByteArray& buffer);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    void encodePacket(const proto::audio::Packet& packet);

    // Source of the enable/disable command. Resolved through WorkerManager on start.
    QPointer<IpcWorker> ipc_worker_;

    std::unique_ptr<AudioCapturer> capturer_;
    std::unique_ptr<AudioEncoder> encoder_;
    Serializer<proto::audio::HostToClient> serializer_;

    Q_DISABLE_COPY_MOVE(AudioWorker)
};

#endif // HOST_WORKERS_AUDIO_WORKER_H
