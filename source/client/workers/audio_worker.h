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

#include "base/serialization.h"
#include "base/threading/worker.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_legacy.h"

class AudioDecoder;
class AudioPlayer;

class AudioWorker final : public Worker
{
    Q_OBJECT

public:
    AudioWorker();
    ~AudioWorker() final;

    struct Metrics
    {
        qint64 packet_count = 0;
        size_t min_packet = 0;
        size_t max_packet = 0;
        size_t avg_packet = 0;
    };

public slots:
    void onAudioMessage(const QByteArray& buffer);
    void onLegacyMessage(const QByteArray& buffer);

signals:
    // Emitted once a second with the current audio statistics.
    void sig_metrics(const AudioWorker::Metrics& metrics);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer() final;

private:
    void decodePacket(const proto::audio::Packet& packet);

    Parser<proto::audio::HostToClient, proto::legacy::SessionToClient> incoming_message_;

    std::unique_ptr<AudioDecoder> decoder_;
    std::unique_ptr<AudioPlayer> player_;
    proto::audio::Encoding encoding_ = proto::audio::ENCODING_UNKNOWN;

    Metrics metrics_;

    Q_DISABLE_COPY_MOVE(AudioWorker)
};

Q_DECLARE_METATYPE(AudioWorker::Metrics)

#endif // CLIENT_WORKERS_AUDIO_WORKER_H
