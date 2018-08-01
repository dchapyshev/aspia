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
#include "codec/video_util.h"

namespace aspia {

ClientSessionDesktopView::ClientSessionDesktopView(
    ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    desktop_window_ = new DesktopWindow(connect_data_);

    connect(desktop_window_, &DesktopWindow::sendConfig,
            this, &ClientSessionDesktopView::onSendConfig);

    connect(desktop_window_, &DesktopWindow::sendScreen,
            this, &ClientSessionDesktopView::onSendScreen);

    // When the window is closed, we close the session.
    connect(desktop_window_, &DesktopWindow::windowClose,
            this, &ClientSessionDesktopView::closedByUser);
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    delete desktop_window_;
}

void ClientSessionDesktopView::messageReceived(const QByteArray& buffer)
{
    message_.Clear();

    if (!parseMessage(buffer, message_))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (message_.has_video_packet())
    {
        readVideoPacket(message_.video_packet());
    }
    else if (message_.has_config_request())
    {
        readConfigRequest(message_.config_request());
    }
    else if (message_.has_screen_list())
    {
        readScreenList(message_.screen_list());
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

void ClientSessionDesktopView::onSendScreen(const proto::desktop::Screen& screen)
{
    proto::desktop::ClientToHost message;
    message.mutable_screen()->CopyFrom(screen);
    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopView::readConfigRequest(
    const proto::desktop::ConfigRequest& /* config_request */)
{
    onSendConfig(connect_data_->desktopConfig());
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
        QRect screen_rect = VideoUtil::fromVideoRect(packet.format().screen_rect());

        if (screen_rect.width()  <= 0 || screen_rect.width()  >= 65535 ||
            screen_rect.height() <= 0 || screen_rect.height() >= 65535)
        {
            emit errorOccurred(tr("Session error: Wrong video frame size."));
            return;
        }

        if (screen_rect.x() < 0 || screen_rect.x() >= 65535 ||
            screen_rect.y() < 0 || screen_rect.y() >= 65535)
        {
            emit errorOccurred(tr("Session error: Wrong video frame position."));
            return;
        }

        desktop_window_->resizeDesktopFrame(screen_rect);
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

void ClientSessionDesktopView::readScreenList(const proto::desktop::ScreenList& screen_list)
{
    desktop_window_->setScreenList(screen_list);
}

} // namespace aspia
