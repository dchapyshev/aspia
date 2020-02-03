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
#include "host/desktop_session.h"

namespace host {

DesktopSessionProxy::DesktopSessionProxy() = default;

DesktopSessionProxy::~DesktopSessionProxy()
{
    DCHECK(!desktop_session_);
}

void DesktopSessionProxy::startSession()
{
    if (desktop_session_)
        desktop_session_->startSession();
}

void DesktopSessionProxy::stopSession()
{
    if (desktop_session_)
        desktop_session_->stopSession();
}

void DesktopSessionProxy::selectScreen(const proto::Screen& screen)
{
    if (desktop_session_)
        desktop_session_->selectScreen(screen);
}

void DesktopSessionProxy::injectKeyEvent(const proto::KeyEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectKeyEvent(event);
}

void DesktopSessionProxy::injectPointerEvent(const proto::PointerEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectPointerEvent(event);
}

void DesktopSessionProxy::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (desktop_session_)
        desktop_session_->injectClipboardEvent(event);
}

void DesktopSessionProxy::logoffUserSession()
{
    if (desktop_session_)
        desktop_session_->logoffUserSession();
}

void DesktopSessionProxy::lockUserSession()
{
    if (desktop_session_)
        desktop_session_->lockUserSession();
}

void DesktopSessionProxy::attach(DesktopSession* desktop_session)
{
    desktop_session_ = desktop_session;
    DCHECK(desktop_session_);
}

void DesktopSessionProxy::dettach()
{
    desktop_session_ = nullptr;
}

} // namespace host
