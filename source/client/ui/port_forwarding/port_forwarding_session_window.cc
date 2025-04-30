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

#include "client/ui/port_forwarding/port_forwarding_session_window.h"

#include "base/logging.h"
#include "client/client_port_forwarding.h"
#include "ui_port_forwarding_session_window.h"

Q_DECLARE_METATYPE(client::ClientPortForwarding::Statistics)

namespace client {

//--------------------------------------------------------------------------------------------------
PortForwardingSessionWindow::PortForwardingSessionWindow(
    const proto::port_forwarding::Config& session_config, QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::PortForwardingSessionWindow>()),
      session_config_(session_config)
{
    LOG(LS_INFO) << "Ctor";
    ui->setupUi(this);
}

//--------------------------------------------------------------------------------------------------
PortForwardingSessionWindow::~PortForwardingSessionWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* PortForwardingSessionWindow::createClient()
{
    LOG(LS_INFO) << "Create client";

    ClientPortForwarding* client = new ClientPortForwarding();

    connect(client, &ClientPortForwarding::sig_showSessionWindow,
            this, &PortForwardingSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientPortForwarding::sig_statistics,
            this, &PortForwardingSessionWindow::onStatisticsChanged,
            Qt::QueuedConnection);

    client->setPortForwardingConfig(session_config_);

    ui->label_remote_host_value->setText(QString::fromStdString(session_config_.remote_host()));
    ui->label_remote_port_value->setText(QString::number(session_config_.remote_port()));
    ui->label_local_port_value->setText(QString::number(session_config_.local_port()));

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void PortForwardingSessionWindow::onShowWindow()
{
    LOG(LS_INFO) << "Show window";
    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void PortForwardingSessionWindow::onStatisticsChanged(const ClientPortForwarding::Statistics& statistics)
{
    ui->labelReceivedValue->setText(QString::number(statistics.rx_bytes));
    ui->labelSentValue->setText(QString::number(statistics.tx_bytes));
}

//--------------------------------------------------------------------------------------------------
void PortForwardingSessionWindow::onInternalReset()
{
    // Nothing
}

} // namespace client
