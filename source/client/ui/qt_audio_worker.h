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

#ifndef CLIENT__UI__QT_AUDIO_WORKER_H
#define CLIENT__UI__QT_AUDIO_WORKER_H

#include <mutex>
#include <queue>

#include <QObject>

class QAudioOutput;
class QIODevice;

namespace proto {
class AudioPacket;
} // namespace proto

namespace client {

class QtAudioWorker : public QObject
{
    Q_OBJECT

public:
    QtAudioWorker();
    ~QtAudioWorker();

protected:
    void customEvent(QEvent* event) override;

private:
    friend class QtAudioRenderer;
    bool init();
    void processEvents();

    std::queue<std::unique_ptr<proto::AudioPacket>> incoming_queue_;
    std::mutex incoming_queue_lock_;

    QAudioOutput* audio_output_ = nullptr;
    QIODevice* audio_device_ = nullptr;
};

} // namespace client

#endif // CLIENT__UI__QT_AUDIO_WORKER_H
