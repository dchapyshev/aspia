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

#include "host/screen_updater.h"

#include <QCoreApplication>
#include <QDebug>

#include "codec/cursor_encoder.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_util.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/cursor_capturer_win.h"
#include "desktop_capture/screen_capturer_gdi.h"

namespace aspia {

ScreenUpdater::ScreenUpdater(const proto::desktop::Config& config, QObject* parent)
    : QThread(parent),
      config_(config)
{
    start(QThread::HighPriority);
}

ScreenUpdater::~ScreenUpdater()
{
    terminate_ = true;
    wait();
}

void ScreenUpdater::run()
{
    std::unique_ptr<ScreenCapturer> screen_capturer = std::make_unique<ScreenCapturerGDI>();
    std::unique_ptr<VideoEncoder> video_encoder;

    switch (config_.video_encoding())
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            video_encoder = VideoEncoderVPX::createVP8();
            break;

        case proto::desktop::VIDEO_ENCODING_VP9:
            video_encoder = VideoEncoderVPX::createVP9();
            break;

        case proto::desktop::VIDEO_ENCODING_ZLIB:
            video_encoder = VideoEncoderZLIB::create(
                VideoUtil::fromVideoPixelFormat(config_.pixel_format()),
                                                config_.compress_ratio());
            break;

        default:
            qWarning() << "Unsupported video encoding: " << config_.video_encoding();
            break;
    }

    if (!video_encoder)
    {
        QCoreApplication::postEvent(parent(), new ErrorEvent());
        return;
    }

    std::unique_ptr<CursorCapturer> cursor_capturer;
    std::unique_ptr<CursorEncoder> cursor_encoder;

    if (config_.features() & proto::desktop::FEATURE_CURSOR_SHAPE)
    {
        cursor_capturer = std::make_unique<CursorCapturerWin>();
        cursor_encoder = std::make_unique<CursorEncoder>();
    }

    CaptureScheduler capture_scheduler(std::chrono::milliseconds(config_.update_interval()));

    while (!terminate_)
    {
        capture_scheduler.beginCapture();

        const DesktopFrame* screen_frame = screen_capturer->captureFrame();
        if (screen_frame)
        {
            std::unique_ptr<proto::desktop::VideoPacket> video_packet;
            std::unique_ptr<proto::desktop::CursorShape> cursor_shape;

            if (!screen_frame->constUpdatedRegion().isEmpty())
                video_packet = video_encoder->encode(screen_frame);

            if (cursor_capturer && cursor_encoder)
            {
                std::unique_ptr<MouseCursor> mouse_cursor(cursor_capturer->captureCursor());
                if (mouse_cursor)
                    cursor_shape = cursor_encoder->encode(std::move(mouse_cursor));
            }

            if (video_packet || cursor_shape)
            {
                UpdateEvent* update_event = new UpdateEvent();

                update_event->video_packet = std::move(video_packet);
                update_event->cursor_shape = std::move(cursor_shape);

                QCoreApplication::postEvent(parent(), update_event);
            }
        }

        capture_scheduler.endCapture();

        if (terminate_)
            break;

        std::chrono::milliseconds delay = capture_scheduler.nextCaptureDelay();
        QThread::msleep(delay.count());
    }
}

} // namespace aspia
