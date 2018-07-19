//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__SCREEN_UPDATER_H_
#define ASPIA_HOST__SCREEN_UPDATER_H_

#include <QEvent>
#include <QThread>

#include <condition_variable>
#include <memory>
#include <mutex>

#include "protocol/desktop_session.pb.h"

namespace aspia {

class ScreenUpdater : public QThread
{
    Q_OBJECT

public:
    ScreenUpdater(const proto::desktop::Config& config, QObject* parent);
    ~ScreenUpdater();

    void update();

    class UpdateEvent : public QEvent
    {
    public:
        static const int kType = QEvent::User + 1;

        UpdateEvent()
            : QEvent(static_cast<QEvent::Type>(kType))
        {
            // Nothing
        }

        std::unique_ptr<aspia::proto::desktop::VideoPacket> video_packet;
        std::unique_ptr<aspia::proto::desktop::CursorShape> cursor_shape;

    private:
        Q_DISABLE_COPY(UpdateEvent)
    };

    class ErrorEvent : public QEvent
    {
    public:
        static const int kType = QEvent::User + 2;

        ErrorEvent()
            : QEvent(static_cast<QEvent::Type>(kType))
        {
            // Nothing
        }

    private:
        Q_DISABLE_COPY(ErrorEvent)
    };

protected:
    // QThread implementation.
    void run() override;

private:
    std::condition_variable update_condition_;
    std::mutex update_lock_;
    bool update_required_ = false;
    bool terminate_ = false;

    proto::desktop::Config config_;

    Q_DISABLE_COPY(ScreenUpdater)
};

} // namespace aspia

#endif // ASPIA_HOST__SCREEN_UPDATER_H_
