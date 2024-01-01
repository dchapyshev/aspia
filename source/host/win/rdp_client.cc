//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/rdp_client.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "host/win/rdp_client_window.h"

#include <cstdint>
#include <memory>

#include <windows.h>

namespace host {

namespace {

// 127.0.0.1 is explicitly blocked by the RDP ActiveX control, so we use 127.0.0.2 instead.
const asio::ip::address_v4::bytes_type kRdpLoopbackAddress { 127, 0, 0, 2 };

}  // namespace

// The core of RdpClient is ref-counted since it services calls and notifies
// events on the caller task runner, but runs the ActiveX control on the UI
// task runner.
class RdpClient::Core
    : public std::enable_shared_from_this<Core>,
      public RdpClientWindow::EventHandler
{
public:
    Core(std::shared_ptr<base::TaskRunner> caller_task_runner,
         std::shared_ptr<base::TaskRunner> ui_task_runner,
         RdpClient::EventHandler* event_handler);
    ~Core() override;

    // Initiates a loopback RDP connection.
    void connect(const base::Size& resolution,
                 const std::string& terminal_id,
                 uint16_t port_number);

    // Initiates a graceful shutdown of the RDP connection.
    void disconnect();

    // Sends Secure Attention Sequence to the session.
    void injectSas();

    // Change the resolution of the desktop.
    void changeResolution(const base::Size& resolution);

    // RdpClientWindow::EventHandler interface.
    void onConnected() override;
    void onDisconnected() override;

private:
    // Helpers for the event handler's methods that make sure that onRdpClosed()
    // is the last notification delivered and is delevered only once.
    void notifyConnected();
    void notifyClosed();

    // Task runner on which the caller expects |event_handler_| to be notified.
    std::shared_ptr<base::TaskRunner> caller_task_runner_;

    // Task runner on which |rdp_client_window_| is running.
    std::shared_ptr<base::TaskRunner> ui_task_runner_;

    // Event handler receiving notification about connection state. The pointer is
    // cleared when Disconnect() methods is called, stopping any further updates.
    RdpClient::EventHandler* event_handler_;

    // Hosts the RDP ActiveX control.
    std::unique_ptr<RdpClientWindow> rdp_client_window_;

    // A self-reference to keep the object alive during connection shutdown.
    std::shared_ptr<Core> self_;

    DISALLOW_COPY_AND_ASSIGN(Core);
};

//--------------------------------------------------------------------------------------------------
RdpClient::RdpClient(
    std::shared_ptr<base::TaskRunner> caller_task_runner,
    std::shared_ptr<base::TaskRunner> ui_task_runner,
    const base::Size& resolution,
    const std::string& terminal_id,
    uint32_t port_number,
    EventHandler* event_handler)
{
    DCHECK(caller_task_runner->belongsToCurrentThread());

    core_ = std::make_shared<Core>(caller_task_runner, ui_task_runner, event_handler);
    core_->connect(resolution, terminal_id, port_number);
}

//--------------------------------------------------------------------------------------------------
RdpClient::~RdpClient()
{
    DCHECK_CALLED_ON_VALID_THREAD(sequence_checker_);
    core_->disconnect();
}

//--------------------------------------------------------------------------------------------------
void RdpClient::injectSas()
{
    DCHECK_CALLED_ON_VALID_THREAD(sequence_checker_);
    core_->injectSas();
}

//--------------------------------------------------------------------------------------------------
void RdpClient::changeResolution(const base::Size& resolution)
{
    DCHECK_CALLED_ON_VALID_THREAD(sequence_checker_);
    core_->changeResolution(resolution);
}

//--------------------------------------------------------------------------------------------------
RdpClient::Core::Core(
    std::shared_ptr<base::TaskRunner> caller_task_runner,
    std::shared_ptr<base::TaskRunner> ui_task_runner,
    RdpClient::EventHandler* event_handler)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      event_handler_(event_handler)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::connect(const base::Size& resolution,
                              const std::string& terminal_id,
                              uint16_t port_number)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &Core::connect, shared_from_this(), resolution, terminal_id, port_number));
        return;
    }

    DCHECK(!rdp_client_window_);
    DCHECK(!self_.get());

    asio::ip::tcp::endpoint server_endpoint(asio::ip::address_v4(kRdpLoopbackAddress), port_number);

    // Create the ActiveX control window.
    rdp_client_window_ =
      std::make_unique<RdpClientWindow>(server_endpoint, terminal_id, ui_task_runner_, this);
    if (!rdp_client_window_->connect(resolution))
    {
        rdp_client_window_.reset();

        // Notify the caller that connection attempt failed.
        notifyClosed();
    }
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::disconnect()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Core::disconnect, shared_from_this()));
        return;
    }

    // The caller does not expect any notifications to be delivered after this point.
    event_handler_ = nullptr;

    // Gracefully shutdown the RDP connection.
    if (rdp_client_window_)
    {
        self_ = shared_from_this();
        rdp_client_window_->disconnect();
    }
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::injectSas()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Core::injectSas, shared_from_this()));
        return;
    }

    if (rdp_client_window_)
        rdp_client_window_->injectSas();
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::changeResolution(const base::Size& resolution)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Core::changeResolution, shared_from_this(), resolution));
        return;
    }

    if (rdp_client_window_)
        rdp_client_window_->changeResolution(resolution);
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::onConnected()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(rdp_client_window_);

    notifyConnected();
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::onDisconnected()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(rdp_client_window_);

    notifyClosed();

    // Delay window destruction until no ActiveX control's code is on the stack.
    ui_task_runner_->deleteSoon(std::move(rdp_client_window_));
    self_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
RdpClient::Core::~Core()
{
    DCHECK(!event_handler_);
    DCHECK(!rdp_client_window_);
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::notifyConnected()
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(std::bind(&Core::notifyConnected, shared_from_this()));
        return;
    }

    if (event_handler_)
        event_handler_->onRdpConnected();
}

//--------------------------------------------------------------------------------------------------
void RdpClient::Core::notifyClosed()
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(std::bind(&Core::notifyClosed, shared_from_this()));
        return;
    }

    if (event_handler_)
    {
        RdpClient::EventHandler* event_handler = event_handler_;
        event_handler_ = nullptr;
        event_handler->onRdpClosed();
    }
}

} // namespace host
