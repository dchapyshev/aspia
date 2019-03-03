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

#include "host/screen_updater_impl.h"
#include "codec/cursor_encoder.h"
#include "codec/scale_reducer.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zstd.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "desktop/capture_scheduler.h"
#include "desktop/cursor_capturer_win.h"
#include "desktop/screen_capturer_gdi.h"
#include "proto/desktop_session_extensions.pb.h"

#include <QCoreApplication>

namespace host {

ScreenUpdaterImpl::ScreenUpdaterImpl(QObject* parent)
    : QThread(parent)
{
    // Nothing
}

ScreenUpdaterImpl::~ScreenUpdaterImpl()
{
    // Set the event.
    event_lock_.lock();
    event_ = Event::TERMINATE;
    event_lock_.unlock();

    // Notify the thread about the event.
    event_condition_.notify_all();

    // Waiting for the completion of the thread.
    wait();
}

bool ScreenUpdaterImpl::startUpdater(const proto::desktop::Config& config)
{
    scale_reducer_.reset(codec::ScaleReducer::create(config.scale_factor()));
    if (!scale_reducer_)
        return false;

    switch (config.video_encoding())
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            video_encoder_.reset(codec::VideoEncoderVPX::createVP8());
            break;

        case proto::desktop::VIDEO_ENCODING_VP9:
            video_encoder_.reset(codec::VideoEncoderVPX::createVP9());
            break;

        case proto::desktop::VIDEO_ENCODING_ZSTD:
            video_encoder_.reset(codec::VideoEncoderZstd::create(
                codec::VideoUtil::fromVideoPixelFormat(
                    config.pixel_format()), config.compress_ratio()));
            break;

        default:
        {
            // No supported video encoding.
            LOG(LS_WARNING) << "Unsupported video encoding: " << config.video_encoding();
            video_encoder_.reset();
        }
        break;
    }

    if (!video_encoder_)
        return false;

    if (config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
    {
        cursor_capturer_.reset(new desktop::CursorCapturerWin());
        cursor_encoder_.reset(new codec::CursorEncoder());
    }

    capture_scheduler_.reset(
        new desktop::CaptureScheduler(std::chrono::milliseconds(config.update_interval())));

    if (config.flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS)
        screen_capturer_flags_ |= desktop::ScreenCapturerGDI::DISABLE_EFFECTS;

    if (config.flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER)
        screen_capturer_flags_ |= desktop::ScreenCapturerGDI::DISABLE_WALLPAPER;

    start(QThread::HighPriority);
    return true;
}

void ScreenUpdaterImpl::selectScreen(desktop::ScreenCapturer::ScreenId screen_id)
{
    // Set the event.
    std::scoped_lock lock(event_lock_);
    event_ = Event::SELECT_SCREEN;
    screen_id_ = screen_id;

    // Notify the thread about the event.
    event_condition_.notify_all();
}

void ScreenUpdaterImpl::run()
{
    screen_capturer_.reset(new desktop::ScreenCapturerGDI(screen_capturer_flags_));

    while (true)
    {
        int count = screen_capturer_->screenCount();
        if (screen_count_ != count)
        {
            std::scoped_lock lock(event_lock_);

            screen_count_ = count;

            // The list of screens has changed. We do not know which screen is removed or added.
            // We display the full desktop and send a new list of screens.
            screen_id_ = desktop::ScreenCapturer::kFullDesktopScreenId;

            desktop::ScreenCapturer::ScreenList screens;
            if (screen_capturer_->screenList(&screens))
            {
                proto::desktop::ScreenList screen_list;

                for (const auto& screen : screens)
                {
                    proto::desktop::Screen* item = screen_list.add_screen();

                    item->set_id(screen.id);
                    item->set_title(screen.title.toStdString());
                }

                message_.Clear();

                proto::desktop::Extension* extension = message_.mutable_extension();

                extension->set_name(common::kSelectScreenExtension);
                extension->set_data(screen_list.SerializeAsString());

                QCoreApplication::postEvent(
                    parent(), new MessageEvent(common::serializeMessage(message_)));
            }

            screen_capturer_->selectScreen(screen_id_);
        }

        capture_scheduler_->beginCapture();

        const desktop::Frame* screen_frame = screen_capturer_->captureFrame();
        if (screen_frame)
        {
            message_.Clear();

            if (!screen_frame->constUpdatedRegion().isEmpty())
            {
                video_encoder_->encode(scale_reducer_->scaleFrame(screen_frame),
                                       message_.mutable_video_packet());
            }

            if (cursor_capturer_ && cursor_encoder_)
            {
                std::unique_ptr<desktop::MouseCursor> mouse_cursor(
                    cursor_capturer_->captureCursor());
                if (mouse_cursor)
                {
                    cursor_encoder_->encode(std::move(mouse_cursor),
                                            message_.mutable_cursor_shape());
                }
            }

            if (message_.has_video_packet() || message_.has_cursor_shape())
            {
                QCoreApplication::postEvent(parent(),
                                        new MessageEvent(common::serializeMessage(message_)),
                                        Qt::HighEventPriority);
            }
        }

        capture_scheduler_->endCapture();

        std::unique_lock lock(event_lock_);
        event_condition_.wait_for(lock, capture_scheduler_->nextCaptureDelay());

        switch (event_)
        {
            case Event::NO_EVENT:
                break;

            case Event::TERMINATE:
                return;

            case Event::SELECT_SCREEN:
                screen_capturer_->selectScreen(screen_id_);
                break;
        }

        event_ = Event::NO_EVENT;
    }
}

} // namespace host
