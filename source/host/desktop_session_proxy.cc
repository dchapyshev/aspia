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

DesktopSessionProxy::DesktopSessionProxy()
{
    resetFeatures();
}

DesktopSessionProxy::~DesktopSessionProxy()
{
    DCHECK(!desktop_session_);
}

void DesktopSessionProxy::restoreState()
{
    enableFeatures(features_);
}

void DesktopSessionProxy::enableSession(bool enable)
{
    if (!enable)
        resetFeatures();

    if (desktop_session_)
        desktop_session_->enableSession(enable);
}

void DesktopSessionProxy::selectScreen(const proto::Screen& screen)
{
    if (desktop_session_)
        desktop_session_->selectScreen(screen);
}

void DesktopSessionProxy::enableFeatures(const proto::internal::EnableFeatures& features)
{
    mergeFeatures(features);
 
    if (desktop_session_)
        desktop_session_->enableFeatures(features_);
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

void DesktopSessionProxy::userSessionControl(proto::internal::UserSessionControl::Action action)
{
    if (desktop_session_)
        desktop_session_->userSessionControl(action);
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

void DesktopSessionProxy::mergeFeatures(const proto::internal::EnableFeatures& features)
{
    // If at least one client has disabled effects, then the effects will be disabled for everyone.
    if (!features.effects() || !features_.effects())
        features_.set_effects(false);
    else
        features_.set_effects(true);

    // If at least one client has disabled the wallpaper, then the effects will be disabled for
    // everyone.
    if (!features.wallpaper() || !features_.wallpaper())
        features_.set_wallpaper(false);
    else
        features_.set_wallpaper(true);

    // If at least one client has enabled input block, then the block will be enabled for everyone.
    if (features.block_input() || features_.block_input())
        features_.set_block_input(true);
    else
        features_.set_block_input(false);
}

void DesktopSessionProxy::resetFeatures()
{
    features_.set_wallpaper(true);
    features_.set_effects(true);
    features_.set_block_input(false);
}

} // namespace host
