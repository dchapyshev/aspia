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

#include "client/ui/qt_audio_renderer.h"

#include "base/logging.h"
#include "client/ui/qt_audio_worker.h"
#include "proto/desktop.pb.h"

#include <QApplication>
#include <QAudioOutput>
#include <QEvent>
#include <QTimer>

namespace client {

QtAudioRenderer::QtAudioRenderer()
{
    thread_ = new QThread(this);
    worker_ = new QtAudioWorker();

    worker_->moveToThread(thread_);
    thread_->start(QThread::HighPriority);
}

QtAudioRenderer::~QtAudioRenderer()
{
    thread_->terminate();
    thread_->wait();
}

void QtAudioRenderer::addAudioPacket(std::unique_ptr<proto::AudioPacket> packet)
{
    bool schedule_event = false;

    {
        std::scoped_lock lock(worker_->incoming_queue_lock_);
        schedule_event = worker_->incoming_queue_.empty();
        worker_->incoming_queue_.emplace(std::move(packet));
    }

    if (schedule_event)
        QApplication::postEvent(worker_, new QEvent(QEvent::Type(QEvent::User + 1)));
}

} // namespace client
