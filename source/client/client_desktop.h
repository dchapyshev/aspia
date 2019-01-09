//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__CLIENT_DESKTOP_H
#define ASPIA_CLIENT__CLIENT_DESKTOP_H

#include "client/client.h"
#include "protocol/desktop_session_extensions.pb.h"

namespace aspia {

class CursorDecoder;
class DesktopFrame;
class VideoDecoder;

class ClientDesktop : public Client
{
    Q_OBJECT

public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void resizeDesktopFrame(const QRect& screen_rect) = 0;
        virtual void drawDesktopFrame() = 0;
        virtual DesktopFrame* desktopFrame() = 0;
        virtual void injectCursor(const QCursor& cursor) = 0;
        virtual void injectClipboard(const proto::desktop::ClipboardEvent& event) = 0;
        virtual void setScreenList(const proto::desktop::ScreenList& screen_list) = 0;
    };

    ClientDesktop(const ConnectData& connect_data, Delegate* delegate, QObject* parent);
    ~ClientDesktop();

    void sendKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void sendPointerEvent(const QPoint& pos, uint32_t mask);
    void sendClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void sendPowerControl(proto::desktop::PowerControl::Action action);
    void sendConfig(const proto::desktop::Config& config);
    void sendScreen(const proto::desktop::Screen& screen);

protected:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;

private:
    void readConfigRequest(const proto::desktop::ConfigRequest& config_request);
    void readVideoPacket(const proto::desktop::VideoPacket& packet);
    void readCursorShape(const proto::desktop::CursorShape& cursor_shape);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event);
    void readExtension(const proto::desktop::Extension& extension);

    Delegate* delegate_;

    proto::desktop::HostToClient incoming_message_;
    proto::desktop::ClientToHost outgoing_message_;

    proto::desktop::VideoEncoding video_encoding_ = proto::desktop::VIDEO_ENCODING_UNKNOWN;
    std::unique_ptr<VideoDecoder> video_decoder_;
    std::unique_ptr<CursorDecoder> cursor_decoder_;

    DISALLOW_COPY_AND_ASSIGN(ClientDesktop);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_DESKTOP_H
