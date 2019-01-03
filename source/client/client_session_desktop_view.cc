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

#include "client/ui/desktop_window.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"

namespace aspia {

ClientSessionDesktopView::ClientSessionDesktopView(ConnectData* connect_data, QObject* parent)
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

    connect(desktop_window_, &DesktopWindow::windowClose,
            desktop_window_, &DesktopWindow::deleteLater);
}

void ClientSessionDesktopView::messageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!parseMessage(buffer, incoming_message_))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (incoming_message_.has_video_packet())
    {
        readVideoPacket(incoming_message_.video_packet());
    }
    else if (incoming_message_.has_config_request())
    {
        readConfigRequest(incoming_message_.config_request());
    }
    else if (incoming_message_.has_extension())
    {
        readExtension(incoming_message_.extension());
    }
    else
    {
        // Unknown messages are ignored.
        LOG(LS_WARNING) << "Unhandled message from host";
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
    outgoing_message_.Clear();
    outgoing_message_.mutable_config()->CopyFrom(config);
    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopView::onSendScreen(const proto::desktop::Screen& screen)
{
    outgoing_message_.Clear();

    proto::desktop::Extension* extension = outgoing_message_.mutable_extension();

    extension->set_name(kSelectScreenExtension);
    extension->set_data(screen.SerializeAsString());

    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopView::readConfigRequest(
    const proto::desktop::ConfigRequest& /* config_request */)
{
    onSendConfig(connect_data_->desktop_config);
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

        static const int kMaxValue = std::numeric_limits<uint16_t>::max();
        static const int kMinValue = -std::numeric_limits<uint16_t>::max();

        if (screen_rect.width()  <= 0 || screen_rect.width()  >= kMaxValue ||
            screen_rect.height() <= 0 || screen_rect.height() >= kMaxValue)
        {
            emit errorOccurred(tr("Session error: Wrong video frame size."));
            return;
        }

        if (screen_rect.x() < kMinValue || screen_rect.x() >= kMaxValue ||
            screen_rect.y() < kMinValue || screen_rect.y() >= kMaxValue)
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

void ClientSessionDesktopView::readExtension(const proto::desktop::Extension& extension)
{
    if (extension.name() == kSelectScreenExtension)
    {
        proto::desktop::ScreenList screen_list;

        if (!screen_list.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse select screen extension data";
            return;
        }

        desktop_window_->setScreenList(screen_list);
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

} // namespace aspia
