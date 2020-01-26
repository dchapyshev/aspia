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

#ifndef HOST__CLIENT_SESSION_DESKTOP_H
#define HOST__CLIENT_SESSION_DESKTOP_H

#include "base/macros_magic.h"
#include "host/client_session.h"
#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

namespace desktop {
class Frame;
} // namespace desktop

namespace host {

class DesktopSessionProxy;
class VideoFramePump;

class ClientSessionDesktop : public ClientSession
{
public:
    ClientSessionDesktop(proto::SessionType session_type, std::unique_ptr<net::Channel> channel);
    ~ClientSessionDesktop();

    void setDesktopSessionProxy(std::shared_ptr<DesktopSessionProxy> desktop_session_proxy);

    void encodeFrame(const desktop::Frame& frame);
    void setScreenList(const proto::ScreenList& list);
    void injectClipboardEvent(const proto::ClipboardEvent& event);

protected:
    // net::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

    // ClientSession implementation.
    void onStarted() override;

private:
    void readExtension(const proto::DesktopExtension& extension);
    void readConfig(const proto::DesktopConfig& config);

    std::shared_ptr<DesktopSessionProxy> desktop_session_proxy_;
    std::unique_ptr<VideoFramePump> frame_pump_;
    std::vector<std::string> extensions_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktop);
};

} // namespace host

#endif // HOST__CLIENT_SESSION_DESKTOP_H
