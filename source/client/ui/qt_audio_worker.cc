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

#include "client/ui/qt_audio_worker.h"

#include "base/logging.h"
#include "proto/desktop.pb.h"

#include <QAudioOutput>
#include <QEvent>

namespace client {

QtAudioWorker::QtAudioWorker() = default;

QtAudioWorker::~QtAudioWorker() = default;

void QtAudioWorker::customEvent(QEvent* event)
{
    if (event->type() != QEvent::User + 1)
        return;

    processEvents();
}

bool QtAudioWorker::init()
{
    if (audio_output_ && audio_device_)
        return true;

    QAudioFormat audio_format;
    audio_format.setCodec("audio/pcm");
    audio_format.setChannelCount(2);
    audio_format.setSampleRate(48000);
    audio_format.setSampleSize(16);
    audio_format.setSampleType(QAudioFormat::SignedInt);

    audio_output_ = new QAudioOutput(QAudioDeviceInfo::defaultOutputDevice(), audio_format, this);
    if (audio_output_->error() != QAudio::NoError)
    {
        LOG(LS_WARNING) << "Failed to open audio device";
        return false;
    }

    audio_device_ = audio_output_->start();
    if (!audio_device_)
    {
        LOG(LS_WARNING) << "Audio device not available";
        return false;
    }

    return true;
}

void QtAudioWorker::processEvents()
{
    if (!init())
        return;

    std::queue<std::unique_ptr<proto::AudioPacket>> work_queue;

    {
        std::unique_lock lock(incoming_queue_lock_);
        if (incoming_queue_.empty())
            return;

        work_queue.swap(incoming_queue_);
    }

    while (!work_queue.empty())
    {
        const proto::AudioPacket& packet = *work_queue.front();

        for (int i = 0; i < packet.data_size(); ++i)
        {
            const std::string& data = packet.data(i);
            audio_device_->write(data.data(), data.size());
        }

        work_queue.pop();
    }
}

} // namespace client
