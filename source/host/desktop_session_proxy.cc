//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/desktop_session_proxy.h"

#include "base/logging.h"

namespace host {

DesktopSessionProxy::DesktopSessionProxy() = default;

DesktopSessionProxy::~DesktopSessionProxy()
{
    DCHECK(!desktop_session_);
}

void DesktopSessionProxy::control(proto::internal::Control::Action action)
{
    if (desktop_session_)
        desktop_session_->control(action);
}

void DesktopSessionProxy::configure(const DesktopSession::Config& config)
{
    if (desktop_session_)
        desktop_session_->configure(config);
}

void DesktopSessionProxy::selectScreen(const proto::Screen& screen)
{
    if (desktop_session_)
        desktop_session_->selectScreen(screen);
}

void DesktopSessionProxy::captureScreen()
{
    if (desktop_session_)
        desktop_session_->captureScreen();
}

void DesktopSessionProxy::injectKeyEvent(const proto::KeyEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectKeyEvent(event);
}

void DesktopSessionProxy::injectMouseEvent(const proto::MouseEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectMouseEvent(event);
}

void DesktopSessionProxy::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectClipboardEvent(event);
}

void DesktopSessionProxy::attachAndStart(DesktopSession* desktop_session)
{
    LOG(LS_INFO) << "Desktop session attach";

    desktop_session_ = desktop_session;
    DCHECK(desktop_session_);

    desktop_session_->start();
}

void DesktopSessionProxy::stopAndDettach()
{
    LOG(LS_INFO) << "Desktop session dettach";

    if (desktop_session_)
    {
        desktop_session_->stop();
        desktop_session_ = nullptr;
    }
}

} // namespace host
