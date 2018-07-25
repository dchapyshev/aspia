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

#include "client/client_session_desktop_view.h"

#include "base/message_serialization.h"
#include "client/ui/desktop_window.h"

namespace aspia {

namespace {

const uint32_t kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_ZLIB |
    proto::desktop::VIDEO_ENCODING_VP8 |
    proto::desktop::VIDEO_ENCODING_VP9;

const uint32_t kSupportedFeatures = 0;

} // namespace

ClientSessionDesktopView::ClientSessionDesktopView(
    ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    desktop_window_ = new DesktopWindow(connect_data_);

    connect(desktop_window_, &DesktopWindow::sendConfig,
            this, &ClientSessionDesktopView::onSendConfig);

    // When the window is closed, we close the session.
    connect(desktop_window_, &DesktopWindow::windowClose,
            this, &ClientSessionDesktopView::closedByUser);
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    delete desktop_window_;
}

// static
uint32_t ClientSessionDesktopView::supportedVideoEncodings()
{
    return kSupportedVideoEncodings;
}

// static
uint32_t ClientSessionDesktopView::supportedFeatures()
{
    return kSupportedFeatures;
}

void ClientSessionDesktopView::messageReceived(const QByteArray& buffer)
{
    proto::desktop::HostToClient message;

    if (!parseMessage(buffer, message))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (message.has_video_packet())
    {
        readVideoPacket(message.video_packet());
    }
    else if (message.has_config_request())
    {
        readConfigRequest(message.config_request());
    }
    else
    {
        // Unknown messages are ignored.
        qWarning("Unhandled message from host");
    }
}

void ClientSessionDesktopView::startSession()
{
    desktop_window_->show();
    desktop_window_->activateWindow();
    emit started();
}

void ClientSessionDesktopView::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(desktop_window_, &DesktopWindow::windowClose,
               this, &ClientSessionDesktopView::closedByUser);
    desktop_window_->close();
}

void ClientSessionDesktopView::onSendConfig(const proto::desktop::Config& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopView::readVideoPacket(const proto::desktop::VideoPacket& packet)
{
    if (video_encoding_ != packet.encoding())
    {
        video_decoder_ = VideoDecoder::create(packet.encoding());
        video_encoding_ = packet.encoding();
    }

    if (!video_decoder_)
    {
        emit errorOccurred(tr("Session error: Video decoder not initialized."));
        return;
    }

    if (packet.has_format())
    {
        const proto::desktop::Size& size = packet.format().screen_size();
        const proto::desktop::Point& top_left = packet.format().top_left();

        if (size.width()  <= 0 || size.width()  >= 65535 ||
            size.height() <= 0 || size.height() >= 65535)
        {
            emit errorOccurred(tr("Session error: Wrong video frame size."));
            return;
        }

        if (top_left.x() < 0 || top_left.x() >= 65535 ||
            top_left.y() < 0 || top_left.y() >= 65535)
        {
            emit errorOccurred(tr("Session error: Wrong video frame position."));
            return;
        }

        desktop_window_->resizeDesktopFrame(QPoint(top_left.x(), top_left.y()),
                                            QSize(size.width(), size.height()));
    }

    DesktopFrame* frame = desktop_window_->desktopFrame();
    if (!frame)
    {
        emit errorOccurred(tr("Session error: The desktop frame is not initialized."));
        return;
    }

    if (!video_decoder_->decode(packet, frame))
    {
        emit errorOccurred(tr("Session error: The video packet could not be decoded."));
        return;
    }

    desktop_window_->drawDesktopFrame();
}

void ClientSessionDesktopView::readConfigRequest(
    const proto::desktop::ConfigRequest& config_request)
{
    proto::desktop::Config config = connect_data_->desktopConfig();

    desktop_window_->setSupportedVideoEncodings(config_request.video_encodings());
    desktop_window_->setSupportedFeatures(config_request.features());

    // If current video encoding not supported.
    if (!(config_request.video_encodings() & config.video_encoding()))
    {
        if (!(config_request.video_encodings() & kSupportedVideoEncodings))
        {
            emit errorOccurred(tr("Session error: There are no supported video encodings."));
            return;
        }
        else
        {
            if (!desktop_window_->requireConfigChange(&config))
            {
                emit errorOccurred(tr("Session error: Canceled by the user."));
                return;
            }
        }
    }

    connect_data_->setDesktopConfig(config);
    onSendConfig(config);
}

} // namespace aspia
