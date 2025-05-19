//
// Aspia Project
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

#include "host/desktop_session_proxy.h"

#include "base/logging.h"

#include <thread>

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopSessionProxy::DesktopSessionProxy()
{
    LOG(LS_INFO) << "Ctor";

    if (qEnvironmentVariableIsSet("ASPIA_DEFAULT_FPS"))
    {
        bool ok = false;
        int default_fps = qEnvironmentVariableIntValue("ASPIA_DEFAULT_FPS", &ok);
        if (ok)
        {
            LOG(LS_INFO) << "Default FPS specified by environment variable";

            if (default_fps < 1 || default_fps > 60)
            {
                LOG(LS_INFO) << "Environment variable contains an incorrect default FPS: " << default_fps;
            }
            else
            {
                default_capture_fps_ = default_fps;
            }
        }
    }

    if (qEnvironmentVariableIsSet("ASPIA_MIN_FPS"))
    {
        bool ok = false;
        int min_fps = qEnvironmentVariableIntValue("ASPIA_MIN_FPS", &ok);
        if (ok)
        {
            LOG(LS_INFO) << "Minimum FPS specified by environment variable";

            if (min_fps < 1 || min_fps > 60)
            {
                LOG(LS_INFO) << "Environment variable contains an incorrect minimum FPS: " << min_fps;
            }
            else
            {
                min_capture_fps_ = min_fps;
            }
        }
    }

    bool max_fps_from_env = false;
    if (qEnvironmentVariableIsSet("ASPIA_MAX_FPS"))
    {
        bool ok = false;
        int max_fps = qEnvironmentVariableIntValue("ASPIA_MAX_FPS", &ok);
        if (ok)
        {
            LOG(LS_INFO) << "Maximum FPS specified by environment variable";

            if (max_fps < 1 || max_fps > 60)
            {
                LOG(LS_INFO) << "Environment variable contains an incorrect maximum FPS: " << max_fps;
            }
            else
            {
                max_capture_fps_ = max_fps;
                max_fps_from_env = true;
            }
        }
    }

    if (!max_fps_from_env)
    {
        quint32 threads = std::thread::hardware_concurrency();
        if (threads <= 2)
        {
            LOG(LS_INFO) << "Low-end CPU detected. Maximum capture FPS: " << kMaxScreenCaptureFpsLowEnd;
            max_capture_fps_ = kMaxScreenCaptureFpsLowEnd;
        }
    }
}

//--------------------------------------------------------------------------------------------------
DesktopSessionProxy::~DesktopSessionProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!desktop_session_);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionProxy::setScreenCaptureFps(int fps)
{
    screen_capture_fps_ = fps;

    if (desktop_session_)
        desktop_session_->setScreenCaptureFps(fps);
}

//--------------------------------------------------------------------------------------------------
int DesktopSessionProxy::defaultScreenCaptureFps() const
{
    return default_capture_fps_;
}

//--------------------------------------------------------------------------------------------------
int DesktopSessionProxy::minScreenCaptureFps() const
{
    return min_capture_fps_;
}

//--------------------------------------------------------------------------------------------------
int DesktopSessionProxy::maxScreenCaptureFps() const
{
    return max_capture_fps_;
}

//--------------------------------------------------------------------------------------------------
int DesktopSessionProxy::screenCaptureFps() const
{
    return screen_capture_fps_;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionProxy::attachAndStart(DesktopSession* desktop_session)
{
    LOG(LS_INFO) << "Desktop session attach";

    desktop_session_ = desktop_session;
    DCHECK(desktop_session_);

    desktop_session_->setScreenCaptureFps(screen_capture_fps_);
    desktop_session_->start();
}

//--------------------------------------------------------------------------------------------------
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
