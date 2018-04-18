//
// PROJECT:         Aspia
// FILE:            host/host_session_desktop.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_DESKTOP_H
#define _ASPIA_HOST__HOST_SESSION_DESKTOP_H

#include <QPointer>
#include <QSharedPointer>
#include <QScopedPointer>

#include "host/host_session.h"
#include "protocol/authorization.pb.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class Clipboard;
class InputInjector;
class ScreenUpdater;

class HostSessionDesktop : public HostSession
{
    Q_OBJECT

public:
    HostSessionDesktop(proto::auth::SessionType session_type, const QString& channel_id);
    ~HostSessionDesktop() = default;

public slots:
    // HostSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;

protected:
    // HostSession implementation.
    void startSession() override;
    void stopSession() override;
    void customEvent(QEvent* event) override;

private slots:
    void clipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void readPointerEvent(const proto::desktop::PointerEvent& event);
    void readKeyEvent(const proto::desktop::KeyEvent& event);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void readConfig(const proto::desktop::Config& config);

    const proto::auth::SessionType session_type_;

    QPointer<ScreenUpdater> screen_updater_;
    QPointer<Clipboard> clipboard_;
    QScopedPointer<InputInjector> input_injector_;

    Q_DISABLE_COPY(HostSessionDesktop)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_DESKTOP_H
