//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/desktop_session_fake.h"

#include "base/logging.h"

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopSessionFake::DesktopSessionFake(QObject* parent)
    : DesktopSession(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopSessionFake::~DesktopSessionFake()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::start()
{
    LOG(INFO) << "Start called for fake session";
    emit sig_desktopSessionStarted();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::control(proto::internal::DesktopControl::Action action)
{
    LOG(INFO) << "CONTROL with action:" << action;

    switch (action)
    {
        case proto::internal::DesktopControl::ENABLE:
            emit sig_screenCaptureError(proto::desktop::VIDEO_ERROR_CODE_TEMPORARY);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::configure(const Config& /* config */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::selectScreen(const proto::desktop::Screen& /* screen */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::captureScreen()
{
    emit sig_screenCaptureError(proto::desktop::VIDEO_ERROR_CODE_TEMPORARY);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::setScreenCaptureFps(int /* fps */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::injectKeyEvent(const proto::desktop::KeyEvent& /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::injectTextEvent(const proto::desktop::TextEvent& /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::injectMouseEvent(const proto::desktop::MouseEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::injectTouchEvent(const proto::desktop::TouchEvent& /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionFake::injectClipboardEvent(const proto::desktop::ClipboardEvent& /* event */)
{
    // Nothing
}

} // namespace host
