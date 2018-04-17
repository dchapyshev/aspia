//
// PROJECT:         Aspia
// FILE:            host/screen_updater.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_updater.h"

#include <QCoreApplication>
#include <QDebug>

#include "codec/cursor_encoder.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_util.h"
#include "desktop_capture/capturer_gdi.h"
#include "desktop_capture/capture_scheduler.h"

namespace aspia {

ScreenUpdater::ScreenUpdater(const proto::desktop::Config& config, QObject* parent)
    : QThread(parent),
      config_(config)
{
    start(QThread::HighPriority);
}

ScreenUpdater::~ScreenUpdater()
{
    {
        std::scoped_lock<std::mutex> lock(update_lock_);
        update_required_ = true;
        terminate_ = true;
        update_condition_.notify_one();
    }

    wait();
}

void ScreenUpdater::update()
{
    std::scoped_lock<std::mutex> lock(update_lock_);
    update_required_ = true;
    update_condition_.notify_one();
}

void ScreenUpdater::run()
{
    std::unique_ptr<Capturer> capturer = CapturerGDI::create();
    if (!capturer)
    {
        QCoreApplication::postEvent(parent(), new ErrorEvent());
        return;
    }

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

    std::unique_ptr<CursorEncoder> cursor_encoder;

    if (config_.flags() & proto::desktop::Config::ENABLE_CURSOR_SHAPE)
        cursor_encoder = std::make_unique<CursorEncoder>();

    CaptureScheduler scheduler;

    while (true)
    {
        scheduler.beginCapture();

        const DesktopFrame* screen_frame = capturer->captureImage();
        if (screen_frame)
        {
            std::unique_ptr<proto::desktop::VideoPacket> video_packet;
            std::unique_ptr<proto::desktop::CursorShape> cursor_shape;

            if (!screen_frame->updatedRegion().isEmpty())
                video_packet = video_encoder->encode(screen_frame);

            if (cursor_encoder)
            {
                std::unique_ptr<MouseCursor> mouse_cursor = capturer->captureCursor();
                if (mouse_cursor)
                    cursor_shape = cursor_encoder->encode(std::move(mouse_cursor));
            }

            if (video_packet || cursor_shape)
            {
                UpdateEvent* update_event = new UpdateEvent();

                update_event->video_packet = std::move(video_packet);
                update_event->cursor_shape = std::move(cursor_shape);

                std::unique_lock<std::mutex> lock(update_lock_);
                update_required_ = false;

                QCoreApplication::postEvent(parent(), update_event);

                while (!update_required_)
                    update_condition_.wait(lock);

                if (terminate_)
                    return;
            }
        }

        std::unique_lock<std::mutex> lock(update_lock_);
        update_required_ = false;

        while (!update_required_)
        {
            std::chrono::milliseconds delay =
                scheduler.nextCaptureDelay(std::chrono::milliseconds(config_.update_interval()));

            if (update_condition_.wait_for(lock, delay) == std::cv_status::timeout)
                break;
        }

        if (terminate_)
            return;
    }
}

} // namespace aspia
