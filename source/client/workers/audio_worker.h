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

#ifndef CLIENT_WORKERS_AUDIO_WORKER_H
#define CLIENT_WORKERS_AUDIO_WORKER_H

#include <memory>

#include "base/threading/worker.h"
#include "proto/desktop_audio.h"

class AudioDecoder;
class AudioPlayer;

class AudioWorker final : public Worker
{
    Q_OBJECT

public:
    AudioWorker();
    ~AudioWorker() final;

public slots:
    void onAudioPacket(std::shared_ptr<proto::audio::Packet> packet);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    std::unique_ptr<AudioDecoder> decoder_;
    std::unique_ptr<AudioPlayer> player_;
    proto::audio::Encoding encoding_ = proto::audio::ENCODING_UNKNOWN;

    Q_DISABLE_COPY_MOVE(AudioWorker)
};

#endif // CLIENT_WORKERS_AUDIO_WORKER_H
