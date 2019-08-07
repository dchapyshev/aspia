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

#ifndef CLIENT__CLIENT_DESKTOP_H
#define CLIENT__CLIENT_DESKTOP_H

#include "client/client.h"
#include "desktop/desktop_geometry.h"
#include "proto/desktop_extensions.pb.h"
#include "proto/system_info.pb.h"

namespace codec {
class CursorDecoder;
class VideoDecoder;
} // namespace codec

namespace desktop {
class Frame;
} // namespace desktop

namespace client {

class ClientDesktop : public Client
{
    Q_OBJECT

public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void extensionListChanged() = 0;
        virtual void configRequered() = 0;

        virtual void setDesktopRect(const desktop::Rect& screen_rect) = 0;
        virtual void drawDesktop() = 0;
        virtual desktop::Frame* desktopFrame() = 0;

        virtual void setRemoteCursor(const QCursor& cursor) = 0;
        virtual void setRemoteClipboard(const proto::desktop::ClipboardEvent& event) = 0;
        virtual void setScreenList(const proto::desktop::ScreenList& screen_list) = 0;
        virtual void setSystemInfo(const proto::system_info::SystemInfo& system_info) = 0;
    };

    ClientDesktop(const ConnectData& connect_data, Delegate* delegate, QObject* parent);
    ~ClientDesktop();

    const QStringList& supportedExtensions() const { return supported_extensions_; }
    uint32_t supportedVideoEncodings() const { return supported_video_encodings_; }

    void sendKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void sendPointerEvent(const QPoint& pos, uint32_t mask);
    void sendClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void sendPowerControl(proto::desktop::PowerControl::Action action);
    void sendConfig(const proto::desktop::Config& config);
    void sendScreen(const proto::desktop::Screen& screen);
    void sendRemoteUpdate();
    void sendSystemInfoRequest();

protected:
    // net::Listener implementation.
    void onNetworkMessage(const QByteArray& buffer) override;

private:
    void readConfigRequest(const proto::desktop::ConfigRequest& config_request);
    void readVideoPacket(const proto::desktop::VideoPacket& packet);
    void readCursorShape(const proto::desktop::CursorShape& cursor_shape);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event);
    void readExtension(const proto::desktop::Extension& extension);

    void onSessionError(const QString& message);

    Delegate* delegate_;

    proto::desktop::HostToClient incoming_message_;
    proto::desktop::ClientToHost outgoing_message_;

    QStringList supported_extensions_;
    uint32_t supported_video_encodings_ = 0;

    proto::desktop::VideoEncoding video_encoding_ = proto::desktop::VIDEO_ENCODING_UNKNOWN;
    std::unique_ptr<codec::VideoDecoder> video_decoder_;
    std::unique_ptr<codec::CursorDecoder> cursor_decoder_;

    DISALLOW_COPY_AND_ASSIGN(ClientDesktop);
};

} // namespace client

#endif // CLIENT__CLIENT_DESKTOP_H
