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

#ifndef CONSOLE_FAST_CONNECT_DIALOG_H
#define CONSOLE_FAST_CONNECT_DIALOG_H

#include <QDialog>

#include "base/macros_magic.h"
#include "client/client_config.h"
#include "proto/address_book.h"
#include "ui_fast_connect_dialog.h"

class QAbstractButton;

namespace console {

class FastConnectDialog final : public QDialog
{
    Q_OBJECT

public:
    FastConnectDialog(QWidget* parent,
                      const QString& address_book_guid,
                      const proto::address_book::ComputerGroupConfig& default_config,
                      const std::optional<client::RouterConfig>& router_config);
    ~FastConnectDialog() final;

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void readState();
    void writeState();

    struct State
    {
        QStringList history;
        proto::peer::SessionType session_type;
        proto::desktop::Config desktop_manage_config;
        proto::desktop::Config desktop_view_config;
        proto::port_forwarding::Config port_forwarding_config;
    };

    Ui::FastConnectDialog ui;
    QString address_book_guid_;
    proto::address_book::ComputerGroupConfig default_config_;
    std::optional<client::RouterConfig> router_config_;
    State state_;

    DISALLOW_COPY_AND_ASSIGN(FastConnectDialog);
};

} // namespace console

#endif // CONSOLE_FAST_CONNECT_DIALOG_H
