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

#include "host/host_session_fake_desktop.h"

#include <QPainter>

#include "base/message_serialization.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_util.h"
#include "desktop_capture/desktop_frame_qimage.h"

namespace aspia {

namespace {

const uint32_t kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_ZLIB |
    proto::desktop::VIDEO_ENCODING_VP8 |
    proto::desktop::VIDEO_ENCODING_VP9;

} // namespace

HostSessionFakeDesktop::HostSessionFakeDesktop(QObject* parent)
    : HostSessionFake(parent)
{
    // Nothing
}

void HostSessionFakeDesktop::startSession()
{
    proto::desktop::HostToClient message;
    message.mutable_config_request()->set_video_encodings(kSupportedVideoEncodings);
    emit sendMessage(serializeMessage(message));
}

void HostSessionFakeDesktop::onMessageReceived(const QByteArray& buffer)
{
    proto::desktop::ClientToHost message;

    if (!parseMessage(buffer, message))
    {
        qWarning("Unable to parse message");
        emit errorOccurred();
        return;
    }

    if (message.has_config())
    {
        std::unique_ptr<VideoEncoder> video_encoder(createEncoder(message.config()));
        if (!video_encoder)
        {
            qWarning("Unable to create video encoder");
            emit errorOccurred();
            return;
        }

        std::unique_ptr<DesktopFrame> frame = createFrame();
        if (!frame)
        {
            qWarning("Unable to create video frame");
            emit errorOccurred();
            return;
        }

        proto::desktop::HostToClient message;
        video_encoder->encode(frame.get(), message.mutable_video_packet());
        emit sendMessage(serializeMessage(message));
    }
    else
    {
        // Other messages are ignored.
    }
}

VideoEncoder* HostSessionFakeDesktop::createEncoder(const proto::desktop::Config& config)
{
    switch (config.video_encoding())
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            return VideoEncoderVPX::createVP8();

        case proto::desktop::VIDEO_ENCODING_VP9:
            return VideoEncoderVPX::createVP9();

        case proto::desktop::VIDEO_ENCODING_ZLIB:
            return VideoEncoderZLIB::create(
                VideoUtil::fromVideoPixelFormat(config.pixel_format()), config.compress_ratio());

        default:
            qWarning() << "Unsupported video encoding: " << config.video_encoding();
            return nullptr;
    }
}

std::unique_ptr<DesktopFrame> HostSessionFakeDesktop::createFrame()
{
    const QRect frame_rect(0, 0, 800, 600);
    const QRect table_rect(200, 250, 400, 100);
    const int border_size = 1;
    const int title_height = 30;

    std::unique_ptr<DesktopFrameQImage> frame = DesktopFrameQImage::create(frame_rect.size());
    if (!frame)
        return nullptr;

    QPainter painter(frame->image());

    QRect title_rect(table_rect.x() + border_size,
                     table_rect.y() + border_size,
                     table_rect.width() - (border_size * 2),
                     title_height);

    QRect message_rect(table_rect.x() + border_size,
                       title_rect.bottom() + border_size,
                       table_rect.width() - (border_size * 2),
                       table_rect.height() - title_height - (border_size * 2));

    painter.fillRect(table_rect, QColor(167, 167, 167));
    painter.fillRect(title_rect, QColor(207, 207, 207));
    painter.fillRect(message_rect, QColor(255, 255, 255));

    QPixmap icon(QStringLiteral(":/icon/main.png"));
    QPoint icon_pos(title_rect.x() + 8, title_rect.y() + (title_height / 2) - (icon.height() / 2));

    title_rect.setLeft(icon_pos.x() + icon.width() + 8);

    painter.setPen(Qt::black);

    painter.drawPixmap(icon_pos, icon);
    painter.drawText(title_rect, Qt::AlignVCenter, QStringLiteral("Aspia"));

    painter.drawText(message_rect,
                     Qt::AlignCenter,
                     tr("The session is temporarily unavailable."));

    *frame->updatedRegion() += frame_rect;
    return frame;
}

} // namespace aspia
