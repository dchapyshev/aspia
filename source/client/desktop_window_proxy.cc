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

#include "client/desktop_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/version.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window.h"
#include "client/frame_factory.h"
#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

namespace client {

class DesktopWindowProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> ui_task_runner, DesktopWindow* desktop_window);
    ~Impl();

    void showWindow(std::shared_ptr<DesktopControlProxy> desktop_control_proxy,
                    const base::Version& peer_version);
    void dettach();

    void configRequired();

    void setCapabilities(const std::string& extensions, uint32_t video_encodings);
    void setScreenList(const proto::ScreenList& screen_list);
    void setSystemInfo(const proto::SystemInfo& system_info);

    std::shared_ptr<desktop::Frame> allocateFrame(const desktop::Size& size);
    void drawFrame(std::shared_ptr<desktop::Frame> frame);
    void drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor);

    void injectClipboardEvent(const proto::ClipboardEvent& event);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    std::unique_ptr<FrameFactory> frame_factory_;
    DesktopWindow* desktop_window_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

DesktopWindowProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> ui_task_runner,
                               DesktopWindow* desktop_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      desktop_window_(desktop_window)
{
    frame_factory_ = desktop_window_->frameFactory();
}

DesktopWindowProxy::Impl::~Impl()
{
    DCHECK(!desktop_window_);
}

void DesktopWindowProxy::Impl::showWindow(
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy, const base::Version& peer_version)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::showWindow, shared_from_this(), desktop_control_proxy, peer_version));
        return;
    }

    if (desktop_window_)
        desktop_window_->showWindow(desktop_control_proxy, peer_version);
}

void DesktopWindowProxy::Impl::dettach()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::dettach, shared_from_this()));
        return;
    }

    desktop_window_ = nullptr;
}

void DesktopWindowProxy::Impl::configRequired()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::configRequired, shared_from_this()));
        return;
    }

    if (desktop_window_)
        desktop_window_->configRequired();
}

void DesktopWindowProxy::Impl::setCapabilities(
    const std::string& extensions, uint32_t video_encodings)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::setCapabilities, shared_from_this(), extensions, video_encodings));
        return;
    }

    if (desktop_window_)
        desktop_window_->setCapabilities(extensions, video_encodings);
}

void DesktopWindowProxy::Impl::setScreenList(const proto::ScreenList& screen_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::setScreenList, shared_from_this(), screen_list));
        return;
    }

    if (desktop_window_)
        desktop_window_->setScreenList(screen_list);
}

void DesktopWindowProxy::Impl::setSystemInfo(const proto::SystemInfo& system_info)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::setSystemInfo, shared_from_this(), system_info));
        return;
    }

    if (desktop_window_)
        desktop_window_->setSystemInfo(system_info);
}

std::shared_ptr<desktop::Frame> DesktopWindowProxy::Impl::allocateFrame(const desktop::Size& size)
{
    return frame_factory_->allocateFrame(size);
}

void DesktopWindowProxy::Impl::drawFrame(std::shared_ptr<desktop::Frame> frame)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::drawFrame, shared_from_this(), frame));
        return;
    }

    if (desktop_window_)
        desktop_window_->drawFrame(frame);
}

void DesktopWindowProxy::Impl::drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::drawMouseCursor, shared_from_this(), mouse_cursor));
        return;
    }

    if (desktop_window_)
        desktop_window_->drawMouseCursor(mouse_cursor);
}

void DesktopWindowProxy::Impl::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::injectClipboardEvent, shared_from_this(), event));
        return;
    }

    if (desktop_window_)
        desktop_window_->injectClipboardEvent(event);
}

DesktopWindowProxy::DesktopWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                       DesktopWindow* desktop_window)
    : impl_(std::make_shared<Impl>(std::move(ui_task_runner), desktop_window))
{
    // Nothing
}

DesktopWindowProxy::~DesktopWindowProxy()
{
    impl_->dettach();
}

// static
std::unique_ptr<DesktopWindowProxy> DesktopWindowProxy::create(
    std::shared_ptr<base::TaskRunner> ui_task_runner, DesktopWindow* desktop_window)
{
    if (!ui_task_runner || !desktop_window)
        return nullptr;

    return std::unique_ptr<DesktopWindowProxy>(
        new DesktopWindowProxy(std::move(ui_task_runner), desktop_window));
}

void DesktopWindowProxy::configRequired()
{
    impl_->configRequired();
}

void DesktopWindowProxy::setCapabilities(
    const std::string& extensions, uint32_t video_encodings)
{
    impl_->setCapabilities(extensions, video_encodings);
}

void DesktopWindowProxy::setScreenList(const proto::ScreenList& screen_list)
{
    impl_->setScreenList(screen_list);
}

void DesktopWindowProxy::setSystemInfo(const proto::SystemInfo& system_info)
{
    impl_->setSystemInfo(system_info);
}

std::shared_ptr<desktop::Frame> DesktopWindowProxy::allocateFrame(const desktop::Size& size)
{
    return impl_->allocateFrame(size);
}

void DesktopWindowProxy::showWindow(
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy, const base::Version& peer_version)
{
    impl_->showWindow(desktop_control_proxy, peer_version);
}

void DesktopWindowProxy::drawFrame(std::shared_ptr<desktop::Frame> frame)
{
    impl_->drawFrame(frame);
}

void DesktopWindowProxy::drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    impl_->drawMouseCursor(mouse_cursor);
}

void DesktopWindowProxy::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    impl_->injectClipboardEvent(event);
}

} // namespace client
