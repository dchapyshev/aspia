//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_view.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop_view.h"

#include "base/message_serialization.h"
#include "client/ui/desktop_window.h"

namespace aspia {

namespace {

enum MessageId { ConfigMessageId };

} // namespace

ClientSessionDesktopView::ClientSessionDesktopView(
    proto::address_book::Computer* computer, QObject* parent)
    : ClientSession(parent),
      computer_(computer)
{
    desktop_window_ = new DesktopWindow(computer_);

    connect(desktop_window_, &DesktopWindow::sendConfig,
            this, &ClientSessionDesktopView::onSendConfig);

    // When the window is closed, we close the session.
    connect(desktop_window_, &DesktopWindow::windowClose,
            this, &ClientSessionDesktopView::closedByUser);
}

ClientSessionDesktopView::~ClientSessionDesktopView() = default;

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

    emit readMessage();
}

void ClientSessionDesktopView::messageWritten(int /* message_id */)
{
    // Nothing
}

void ClientSessionDesktopView::startSession()
{
    desktop_window_->show();
    desktop_window_->activateWindow();

    emit readMessage();
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
    emit writeMessage(ConfigMessageId, serializeMessage(message));
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

        if (size.width() <= 0 || size.height() <= 0)
        {
            emit errorOccurred(tr("Session error: Wrong video frame size."));
            return;
        }

        desktop_window_->resizeDesktopFrame(QSize(size.width(), size.height()));
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
    onSendConfig(computer_->session_config().desktop_view());
}

} // namespace aspia
