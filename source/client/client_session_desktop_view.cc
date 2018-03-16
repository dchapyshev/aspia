//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_view.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop_view.h"

#include "client/ui/desktop_window.h"
#include "protocol/message_serialization.h"

namespace aspia {

ClientSessionDesktopView::ClientSessionDesktopView(proto::Computer* computer, QObject* parent)
    : ClientSession(parent),
      computer_(computer)
{
    desktop_window_ = new DesktopWindow(computer_);

    connect(desktop_window_, SIGNAL(sendConfig(const proto::desktop::Config&)),
            this, SLOT(onSendConfig(const proto::desktop::Config&)));

    // When the window is closed, we close the session.
    connect(desktop_window_, SIGNAL(windowClose()), this, SIGNAL(sessionClosed()));

    desktop_window_->show();
    desktop_window_->activateWindow();
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    delete desktop_window_;
}

void ClientSessionDesktopView::readMessage(const QByteArray& buffer)
{
    proto::desktop::HostToClient message;

    if (!ParseMessage(buffer, message))
    {
        emit sessionError(tr("Session error: Invalid message from host."));
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

void ClientSessionDesktopView::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(desktop_window_, SIGNAL(windowClose()), this, SIGNAL(sessionClosed()));
    desktop_window_->close();
}

void ClientSessionDesktopView::onSendConfig(const proto::desktop::Config& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    writeMessage(message);
}

void ClientSessionDesktopView::readVideoPacket(const proto::desktop::VideoPacket& packet)
{
    if (video_encoding_ != packet.encoding())
    {
        video_decoder_ = VideoDecoder::Create(packet.encoding());
        video_encoding_ = packet.encoding();
    }

    if (!video_decoder_)
    {
        emit sessionError(tr("Session error: Video decoder not initialized."));
        return;
    }

    if (packet.has_format())
    {
        const proto::desktop::Size& size = packet.format().screen_size();

        if (size.width() <= 0 || size.height() <= 0)
        {
            emit sessionError(tr("Session error: Wrong video frame size."));
            return;
        }

        desktop_window_->resizeDesktopFrame(QSize(size.width(), size.height()));
    }

    DesktopFrame* frame = desktop_window_->desktopFrame();
    if (!frame)
    {
        emit sessionError(tr("Session error: The desktop frame is not initialized."));
        return;
    }

    if (!video_decoder_->Decode(packet, frame))
    {
        emit sessionError(tr("Session error: The video packet could not be decoded."));
        return;
    }

    desktop_window_->drawDesktopFrame();
}

void ClientSessionDesktopView::writeMessage(const proto::desktop::ClientToHost& message)
{
    emit sessionMessage(SerializeMessage(message));
}

void ClientSessionDesktopView::readConfigRequest(
    const proto::desktop::ConfigRequest& config_request)
{
    onSendConfig(computer_->desktop_view_session());
}

} // namespace aspia
