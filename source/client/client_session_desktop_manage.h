//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_manage.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H

#include "client/client_session_desktop_view.h"
#include "protocol/computer.pb.h"

namespace aspia {

class CursorDecoder;

class ClientSessionDesktopManage : public ClientSessionDesktopView
{
    Q_OBJECT

public:
    ClientSessionDesktopManage(proto::Computer* computer, QObject* parent);

public slots:
    // ClientSession implementation.
    void readMessage(const QByteArray& buffer) override;

    // ClientSessionDesktopView implementation.
    void onSendConfig(const proto::desktop::Config& config) override;

    void onSendKeyEvent(quint32 usb_keycode, quint32 flags);
    void onSendPointerEvent(const QPoint& pos, quint32 mask);
    void onSendClipboardEvent(const QString& text);

private:
    void readConfigRequest(const proto::desktop::ConfigRequest& config_request);
    void readCursorShape(const proto::desktop::CursorShape& cursor_shape);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event);

    std::unique_ptr<CursorDecoder> cursor_decoder_;

    std::string last_clipboard_mime_type_;
    std::string last_clipboard_data_;

    Q_DISABLE_COPY(ClientSessionDesktopManage)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H
