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

#ifndef HOST__HOST_SESSION_DESKTOP_H
#define HOST__HOST_SESSION_DESKTOP_H

#include "host/desktop_config_tracker.h"
#include "host/host_session.h"
#include "host/screen_updater.h"
#include "proto/common.pb.h"
#include "build/build_config.h"

namespace common {
class Clipboard;
} // namespace common

namespace host {

class InputInjector;

class SessionDesktop :
    public Session,
    public ScreenUpdater::Delegate
{
    Q_OBJECT

public:
    SessionDesktop(proto::SessionType session_type, const QString& channel_id);
    ~SessionDesktop();

    // ScreenUpdater::Delegate implementation.
    void onScreenUpdate(const QByteArray& message);

protected:
    // Session implementation.
    void sessionStarted() override;
    void messageReceived(const QByteArray& buffer) override;

private slots:
    void clipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void readPointerEvent(const proto::desktop::PointerEvent& event);
    void readKeyEvent(const proto::desktop::KeyEvent& event);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void readExtension(const proto::desktop::Extension& extension);
    void readConfig(const proto::desktop::Config& config);

    void sendSystemInfo();

    const proto::SessionType session_type_;

    QStringList extensions_;

    proto::desktop::ClientToHost incoming_message_;
    proto::desktop::HostToClient outgoing_message_;

    DesktopConfigTracker config_tracker_;

    std::unique_ptr<ScreenUpdater> screen_updater_;
    std::unique_ptr<common::Clipboard> clipboard_;
    std::unique_ptr<InputInjector> input_injector_;

    DISALLOW_COPY_AND_ASSIGN(SessionDesktop);
};

} // namespace host

#endif // HOST__HOST_SESSION_DESKTOP_H
