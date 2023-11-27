//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/desktop_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/version.h"
#include "base/desktop/geometry.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window.h"
#include "client/frame_factory.h"
#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
DesktopWindowProxy::DesktopWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                       DesktopWindow* desktop_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      desktop_window_(desktop_window)
{
    LOG(LS_INFO) << "Ctor";
    frame_factory_ = desktop_window_->frameFactory();
}

//--------------------------------------------------------------------------------------------------
DesktopWindowProxy::~DesktopWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!desktop_window_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach desktop window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    desktop_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::configRequired()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::configRequired, shared_from_this()));
        return;
    }

    if (desktop_window_)
        desktop_window_->configRequired();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setCapabilities(const proto::DesktopCapabilities& capabilities)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&DesktopWindowProxy::setCapabilities,
                                            shared_from_this(),
                                            capabilities));
        return;
    }

    if (desktop_window_)
        desktop_window_->setCapabilities(capabilities);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setScreenList(const proto::ScreenList& screen_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setScreenList, shared_from_this(), screen_list));
        return;
    }

    if (desktop_window_)
        desktop_window_->setScreenList(screen_list);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setScreenType(const proto::ScreenType& screen_type)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setScreenType, shared_from_this(), screen_type));
        return;
    }

    if (desktop_window_)
        desktop_window_->setScreenType(screen_type);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setCursorPosition(const proto::CursorPosition& cursor_position)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setCursorPosition, shared_from_this(), cursor_position));
        return;
    }

    if (desktop_window_)
        desktop_window_->setCursorPosition(cursor_position);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setSystemInfo, shared_from_this(), system_info));
        return;
    }

    if (desktop_window_)
        desktop_window_->setSystemInfo(system_info);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setTaskManager(const proto::task_manager::HostToClient& message)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setTaskManager, shared_from_this(), message));
        return;
    }

    if (desktop_window_)
        desktop_window_->setTaskManager(message);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setMetrics(const DesktopWindow::Metrics& metrics)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setMetrics, shared_from_this(), metrics));
        return;
    }

    if (desktop_window_)
        desktop_window_->setMetrics(metrics);
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::Frame> DesktopWindowProxy::allocateFrame(const base::Size& size)
{
    return frame_factory_->allocateFrame(size);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::showWindow(
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy, const base::Version& peer_version)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&DesktopWindowProxy::showWindow,
                                            shared_from_this(),
                                            desktop_control_proxy,
                                            peer_version));
        return;
    }

    if (desktop_window_)
        desktop_window_->showWindow(desktop_control_proxy, peer_version);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setFrameError(proto::VideoErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&DesktopWindowProxy::setFrameError,
                                            shared_from_this(),
                                            error_code));
        return;
    }

    if (desktop_window_)
        desktop_window_->setFrameError(error_code);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setFrame(
    const base::Size& screen_size, std::shared_ptr<base::Frame> frame)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setFrame, shared_from_this(), screen_size, frame));
        return;
    }

    if (desktop_window_)
        desktop_window_->setFrame(screen_size, frame);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::drawFrame()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&DesktopWindowProxy::drawFrame, shared_from_this()));
        return;
    }

    if (desktop_window_)
        desktop_window_->drawFrame();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindowProxy::setMouseCursor(std::shared_ptr<base::MouseCursor> mouse_cursor)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&DesktopWindowProxy::setMouseCursor, shared_from_this(), mouse_cursor));
        return;
    }

    if (desktop_window_)
        desktop_window_->setMouseCursor(mouse_cursor);
}

} // namespace client
