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

#ifndef CLIENT_WORKERS_RECORD_WORKER_H
#define CLIENT_WORKERS_RECORD_WORKER_H

#include <memory>

#include "base/serialization.h"
#include "base/desktop/shared_frame.h"
#include "base/threading/worker.h"
#include "proto/desktop_audio.h"

namespace proto::audio {
class Packet;
} // namespace proto::audio

class FrameAligned;
class QTimer;
class WebmFileWriter;
class WebmVideoEncoder;

class RecordWorker final : public Worker
{
    Q_OBJECT

public:
    RecordWorker();
    ~RecordWorker() final;

public slots:
    // Opens or closes the output file.
    void onSetRecording(bool enable, const QString& file_path, const QString& computer_name);

    // Parses one audio channel message and appends its packet to the recording.
    void onAudioMessage(const QByteArray& buffer);

    // Appends one packet to the recording. A no-op while recording is off.
    void onAudioPacket(std::shared_ptr<proto::audio::Packet> packet);

    // Keeps the handle to the current rendered ARGB frame (the same one shown in the GUI).
    void onFrameChanged(const QSize& screen_size, SharedFrame frame);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onEncodeTimer();

private:
    Parser<proto::audio::HostToClient> incoming_message_;
    std::unique_ptr<WebmFileWriter> writer_;

    QTimer* encode_timer_ = nullptr;
    std::unique_ptr<WebmVideoEncoder> encoder_;

    SharedFrame last_frame_;
    std::unique_ptr<FrameAligned> record_frame_;

    Q_DISABLE_COPY_MOVE(RecordWorker)
};

#endif // CLIENT_WORKERS_RECORD_WORKER_H
