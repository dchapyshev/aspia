//
// PROJECT:         Aspia
// FILE:            host/host_notifier.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_notifier.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(Q_OS_WIN)

#include <QScreen>

#include "base/message_serialization.h"
#include "protocol/notifier.pb.h"

namespace aspia {

HostNotifier::HostNotifier(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostNotifier::~HostNotifier() = default;

QString HostNotifier::channelId() const
{
    return channel_id_;
}

void HostNotifier::setChannelId(const QString& channel_id)
{
    channel_id_ = channel_id;
}

bool HostNotifier::start()
{
    if (channel_id_.isEmpty())
    {
        qWarning("Invalid IPC channel id");
        return false;
    }

    ipc_channel_ = IpcChannel::createClient(this);

    connect(ipc_channel_, &IpcChannel::connected, this, &HostNotifier::onIpcChannelConnected);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &HostNotifier::stop);
    connect(ipc_channel_, &IpcChannel::errorOccurred, this, &HostNotifier::stop);
    connect(ipc_channel_, &IpcChannel::messageReceived, this, &HostNotifier::onIpcMessageReceived);

    ipc_channel_->connectToServer(channel_id_);
    return true;
}

void HostNotifier::stop()
{
    if (ipc_channel_->channelState() == IpcChannel::Connected)
        ipc_channel_->stop();
}

void HostNotifier::killSession(const std::string& uuid)
{
    proto::notifier::NotifierToService message;
    message.mutable_kill_session()->set_uuid(uuid);
    ipc_channel_->writeMessage(-1, serializeMessage(message));
}

void HostNotifier::onIpcChannelConnected()
{
    window_ = new HostNotifierWindow();
    connect(window_, &HostNotifierWindow::killSession, this, &HostNotifier::killSession);
    window_->show();

    QSize screen_size = QApplication::primaryScreen()->availableSize();
    QSize window_size = window_->frameSize();

    window_->move(screen_size.width() - window_size.width(),
                  screen_size.height() - window_size.height());

#if defined(Q_OS_WIN)
    // I not found a way better than using WinAPI function in MS Windows.
    // Flag Qt::WindowStaysOnTopHint works incorrectly.
    SetWindowPos(reinterpret_cast<HWND>(window_->winId()),
                 HWND_TOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else // defined(Q_OS_WIN)
    window.setWindowFlags(
        window.windowFlags() | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    window.show();
#endif // defined(Q_OS_WIN)

    ipc_channel_->readMessage();
}

void HostNotifier::onIpcMessageReceived(const QByteArray& buffer)
{
    proto::notifier::ServiceToNotifier message;
    if (!parseMessage(buffer, message))
    {
        qWarning("Invalid message from service");
        stop();
        return;
    }

    if (message.has_session())
    {
        if (!window_.isNull())
            window_->sessionOpen(message.session());
    }
    else if (message.has_session_close())
    {
        if (!window_.isNull())
            window_->sessionClose(message.session_close());
    }
    else
    {
        qWarning("Unhandled message from service");
    }

    ipc_channel_->readMessage();
}

} // namespace aspia
