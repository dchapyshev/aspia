//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__SCREEN_UPDATER_IMPL_H
#define HOST__SCREEN_UPDATER_IMPL_H

#include "desktop/screen_capturer.h"
#include "proto/desktop_session.pb.h"

#include <QEvent>
#include <QThread>

namespace codec {
class CursorEncoder;
class ScaleReducer;
class VideoEncoder;
} // namespace codec

namespace desktop {
class CaptureScheduler;
class CursorCapturer;
} // namespace desktop

namespace host {

class ScreenUpdaterImpl : public QThread
{
public:
    explicit ScreenUpdaterImpl(QObject* parent);
    ~ScreenUpdaterImpl();

    class MessageEvent : public QEvent
    {
    public:
        static const int kType = QEvent::User + 1;

        MessageEvent(QByteArray&& buffer) noexcept
            : QEvent(static_cast<QEvent::Type>(kType)),
              buffer_(std::move(buffer))
        {
            // Nothing
        }

        const QByteArray& buffer() const { return buffer_; }

    private:
        QByteArray buffer_;
        DISALLOW_COPY_AND_ASSIGN(MessageEvent);
    };

    bool startUpdater(const proto::desktop::Config& config);
    void selectScreen(desktop::ScreenCapturer::ScreenId screen_id);

protected:
    // QThread implementation.
    void run() override;

private:
    enum class Event { NO_EVENT, SELECT_SCREEN, TERMINATE };

    uint32_t screen_capturer_flags_ = 0;

    std::unique_ptr<desktop::CaptureScheduler> capture_scheduler_;

    std::unique_ptr<desktop::ScreenCapturer> screen_capturer_;
    std::unique_ptr<codec::ScaleReducer> scale_reducer_;
    std::unique_ptr<codec::VideoEncoder> video_encoder_;

    std::unique_ptr<desktop::CursorCapturer> cursor_capturer_;
    std::unique_ptr<codec::CursorEncoder> cursor_encoder_;

    // By default, we capture the full screen.
    desktop::ScreenCapturer::ScreenId screen_id_ =
        desktop::ScreenCapturer::kFullDesktopScreenId;
    int screen_count_ = 0;

    Event event_ = Event::NO_EVENT;
    std::condition_variable event_condition_;
    std::mutex event_lock_;

    proto::desktop::HostToClient message_;

    DISALLOW_COPY_AND_ASSIGN(ScreenUpdaterImpl);
};

} // namespace host

#endif // HOST__SCREEN_UPDATER_IMPL_H
