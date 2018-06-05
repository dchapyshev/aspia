//
// PROJECT:         Aspia
// FILE:            host/host_session_fake.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_fake.h"

#include <QPainter>

#include "base/message_serialization.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_util.h"
#include "desktop_capture/desktop_frame_qimage.h"

namespace aspia {

namespace {

const quint32 kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_ZLIB |
    proto::desktop::VIDEO_ENCODING_VP8 |
    proto::desktop::VIDEO_ENCODING_VP9;

const quint32 kSupportedFeatures = 0;

} // namespace

HostSessionFake::HostSessionFake(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

// static
HostSessionFake* HostSessionFake::create(proto::auth::SessionType session_type, QObject* parent)
{
    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            return new HostSessionFake(parent);

        default:
            return nullptr;
    }
}

void HostSessionFake::startSession()
{
    proto::desktop::HostToClient message;

    message.mutable_config_request()->set_video_encodings(kSupportedVideoEncodings);
    message.mutable_config_request()->set_features(kSupportedFeatures);

    emit writeMessage(-1, serializeMessage(message));
}

void HostSessionFake::onMessageReceived(const QByteArray& buffer)
{
    proto::desktop::ClientToHost message;

    if (!parseMessage(buffer, message))
    {
        emit errorOccurred();
        return;
    }

    if (message.has_config())
    {
        std::unique_ptr<VideoEncoder> video_encoder = createEncoder(message.config());
        std::unique_ptr<DesktopFrame> frame = createFrame();

        if (!video_encoder || !frame)
        {
            emit errorOccurred();
            return;
        }

        std::unique_ptr<proto::desktop::VideoPacket> packet = video_encoder->encode(frame.get());
        if (!packet)
        {
            emit errorOccurred();
            return;
        }

        sendPacket(std::move(packet));
    }
    else
    {
        // Other messages are ignored.
    }

    emit readMessage();
}

std::unique_ptr<VideoEncoder> HostSessionFake::createEncoder(const proto::desktop::Config& config)
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

std::unique_ptr<DesktopFrame> HostSessionFake::createFrame()
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

    *frame->mutableUpdatedRegion() += frame_rect;
    return frame;
}

void HostSessionFake::sendPacket(std::unique_ptr<proto::desktop::VideoPacket> packet)
{
    proto::desktop::HostToClient message;
    message.set_allocated_video_packet(packet.release());
    emit writeMessage(-1, serializeMessage(message));
}

} // namespace aspia
