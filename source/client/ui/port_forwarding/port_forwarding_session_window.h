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

#ifndef CLIENT_UI_PORT_FORWARDING_PORT_FORWARDING_SESSION_WINDOW_H
#define CLIENT_UI_PORT_FORWARDING_PORT_FORWARDING_SESSION_WINDOW_H

#include "client/client_port_forwarding.h"
#include "client/ui/session_window.h"
#include "proto/port_forwarding.h"

namespace Ui {
class PortForwardingSessionWindow;
} // namespace Ui

namespace client {

class PortForwardingSessionWindow final : public SessionWindow
{
    Q_OBJECT

public:
    explicit PortForwardingSessionWindow(const proto::port_forwarding::Config& session_config,
                                         QWidget* parent = nullptr);
    ~PortForwardingSessionWindow() final;

    // SessionWindow implementation.
    Client* createClient() final;

public slots:
    void onShowWindow();
    void onStatisticsChanged(const client::ClientPortForwarding::Statistics& statistics);

protected:
    // SessionWindow implementation.
    void onInternalReset() final;

private:
    std::unique_ptr<Ui::PortForwardingSessionWindow> ui;
    proto::port_forwarding::Config session_config_;

    Q_DISABLE_COPY(PortForwardingSessionWindow)
};

} // namespace client

#endif // CLIENT_UI_PORT_FORWARDING_PORT_FORWARDING_SESSION_WINDOW_H
