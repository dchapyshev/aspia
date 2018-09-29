//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/client_dialog.h"

#include "build/build_config.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "client/config_factory.h"

namespace aspia {

ClientDialog::ClientDialog(QWidget* parent)
    : QDialog(parent)
{
    connect_data_.port = DEFAULT_HOST_TCP_PORT;
    connect_data_.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
    connect_data_.desktop_config = ConfigFactory::defaultDesktopManageConfig();

    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    ui.spin_port->setValue(connect_data_.port);

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor-keyboard.png")),
                                   tr("Desktop Manage"),
                                   QVariant(proto::SESSION_TYPE_DESKTOP_MANAGE));

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor.png")),
                                   tr("Desktop View"),
                                   QVariant(proto::SESSION_TYPE_DESKTOP_VIEW));

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/folder-stand.png")),
                                   tr("File Transfer"),
                                   QVariant(proto::SESSION_TYPE_FILE_TRANSFER));

    int current_session_type =
        ui.combo_session_type->findData(QVariant(connect_data_.session_type));
    if (current_session_type != -1)
    {
        ui.combo_session_type->setCurrentIndex(current_session_type);
        sessionTypeChanged(current_session_type);
    }

    connect(ui.combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientDialog::sessionTypeChanged);

    connect(ui.button_session_config, &QPushButton::pressed,
            this, &ClientDialog::sessionConfigButtonPressed);

    connect(ui.button_connect, &QPushButton::pressed,
            this, &ClientDialog::connectButtonPressed);

    ui.edit_address->setFocus();
}

ClientDialog::~ClientDialog() = default;

void ClientDialog::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            ui.button_session_config->setEnabled(true);
            connect_data_.desktop_config = ConfigFactory::defaultDesktopManageConfig();
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            ui.button_session_config->setEnabled(true);
            connect_data_.desktop_config = ConfigFactory::defaultDesktopViewConfig();
        }
        break;

        default:
            ui.button_session_config->setEnabled(false);
            break;
    }
}

void ClientDialog::sessionConfigButtonPressed()
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            DesktopConfigDialog dialog(session_type, connect_data_.desktop_config, this);

            if (dialog.exec() == DesktopConfigDialog::Accepted)
                connect_data_.desktop_config = dialog.config();
        }
        break;

        default:
            break;
    }
}

void ClientDialog::connectButtonPressed()
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_type->currentData().toInt());

    connect_data_.address = ui.edit_address->text().toStdString();
    connect_data_.port = ui.spin_port->value();
    connect_data_.session_type = session_type;

    accept();
    close();
}

} // namespace aspia
