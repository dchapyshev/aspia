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

#ifndef ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H_
#define ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H_

#include "client/client_session_desktop_view.h"

namespace aspia {

class CursorDecoder;

class ClientSessionDesktopManage : public ClientSessionDesktopView
{
    Q_OBJECT

public:
    ClientSessionDesktopManage(ConnectData* connect_data, QObject* parent);

public slots:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;

    // ClientSessionDesktopView implementation.
    void onSendConfig(const proto::desktop::Config& config) override;

    void onSendKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void onSendPointerEvent(const DesktopPoint& pos, uint32_t mask);
    void onSendClipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void readCursorShape(const proto::desktop::CursorShape& cursor_shape);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event);

    std::unique_ptr<CursorDecoder> cursor_decoder_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktopManage);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H_
