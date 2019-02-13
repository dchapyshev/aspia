//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/host_window.h"

#include <QDesktopServices>
#include <QUrl>

#include "common/ui/about_dialog.h"
#include "net/network_adapter_enumerator.h"

namespace host {

HostWindow::HostWindow(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    QString ip_list;

    for (net::AdapterEnumerator adapters; !adapters.isAtEnd(); adapters.advance())
    {
        for (net::AdapterEnumerator::IpAddressEnumerator addresses(adapters);
             !addresses.isAtEnd(); addresses.advance())
        {
            if (!ip_list.isEmpty())
                ip_list += QStringLiteral(", ");

            ip_list += QString::fromStdString(addresses.address());
        }
    }

    ui.edit_ip->setText(ip_list);

    connect(ui.action_help, &QAction::triggered, this, &HostWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &HostWindow::onAbout);
}

HostWindow::~HostWindow() = default;

void HostWindow::onHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void HostWindow::onAbout()
{
    common::AboutDialog(this).exec();
}

} // namespace host
