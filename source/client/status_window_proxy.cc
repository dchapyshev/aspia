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

#include "client/status_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

class StatusWindowProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> ui_task_runner, StatusWindow* status_window);
    ~Impl();

    void onStarted(const std::u16string& address, uint16_t port);
    void onStopped();
    void onConnected();
    void onDisconnected(net::ErrorCode error_code);
    void onAccessDenied(Authenticator::ErrorCode error_code);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    StatusWindow* status_window_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

StatusWindowProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> ui_task_runner,
                              StatusWindow* status_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      status_window_(status_window)
{
    // Nothing
}

StatusWindowProxy::Impl::~Impl()
{
    DCHECK(!status_window_);
}

void StatusWindowProxy::Impl::onStarted(const std::u16string& address, uint16_t port)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::onStarted, shared_from_this(), address, port));
        return;
    }

    if (status_window_)
        status_window_->onStarted(address, port);
}

void StatusWindowProxy::Impl::onStopped()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::onStopped, shared_from_this()));
        return;
    }

    if (status_window_)
    {
        status_window_->onStopped();
        status_window_ = nullptr;
    }
}

void StatusWindowProxy::Impl::onConnected()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::onConnected, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onConnected();
}

void StatusWindowProxy::Impl::onDisconnected(net::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onDisconnected, shared_from_this(), error_code));
        return;
    }

    if (status_window_)
        status_window_->onDisconnected(error_code);
}

void StatusWindowProxy::Impl::onAccessDenied(Authenticator::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onAccessDenied, shared_from_this(), error_code));
        return;
    }

    if (status_window_)
        status_window_->onAccessDenied(error_code);
}

StatusWindowProxy::StatusWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                     StatusWindow* status_window)
    : impl_(std::make_shared<Impl>(std::move(ui_task_runner), status_window))
{
    // Nothing
}

StatusWindowProxy::~StatusWindowProxy()
{
    impl_->onStopped();
}

// static
std::unique_ptr<StatusWindowProxy> StatusWindowProxy::create(
    std::shared_ptr<base::TaskRunner> ui_task_runner, StatusWindow* status_window)
{
    if (!ui_task_runner || !status_window)
        return nullptr;

    return std::unique_ptr<StatusWindowProxy>(
        new StatusWindowProxy(std::move(ui_task_runner), status_window));
}

void StatusWindowProxy::onStarted(const std::u16string& address, uint16_t port)
{
    impl_->onStarted(address, port);
}

void StatusWindowProxy::onStopped()
{
    impl_->onStopped();
}

void StatusWindowProxy::onConnected()
{
    impl_->onConnected();
}

void StatusWindowProxy::onDisconnected(net::ErrorCode error_code)
{
    impl_->onDisconnected(error_code);
}

void StatusWindowProxy::onAccessDenied(Authenticator::ErrorCode error_code)
{
    impl_->onAccessDenied(error_code);
}

} // namespace client
