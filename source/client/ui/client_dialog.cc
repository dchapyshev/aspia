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
#include "client/config_factory.h"
#include "common/desktop_session_constants.h"
#include "common/session_type.h"
#include "net/address.h"

#include <QMessageBox>

namespace client {

ClientDialog::ClientDialog(QWidget* parent)
    : QDialog(parent)
{
    connect_data_.port = DEFAULT_HOST_TCP_PORT;
    connect_data_.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
    connect_data_.desktop_config = ConfigFactory::defaultDesktopManageConfig();

    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    auto add_session = [this](const QString& icon, proto::SessionType session_type)
    {
        ui.combo_session_type->addItem(QIcon(icon),
                                       common::sessionTypeToLocalizedString(session_type),
                                       QVariant(session_type));
    };

    add_session(QStringLiteral(":/img/monitor-keyboard.png"), proto::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(QStringLiteral(":/img/monitor.png"), proto::SESSION_TYPE_DESKTOP_VIEW);
    add_session(QStringLiteral(":/img/folder-stand.png"), proto::SESSION_TYPE_FILE_TRANSFER);

    int current_session_type =
        ui.combo_session_type->findData(QVariant(connect_data_.session_type));
    if (current_session_type != -1)
    {
        ui.combo_session_type->setCurrentIndex(current_session_type);
        sessionTypeChanged(current_session_type);
    }

    connect(ui.combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientDialog::sessionTypeChanged);

    connect(ui.button_session_config, &QPushButton::released,
            this, &ClientDialog::sessionConfigButtonPressed);

    connect(ui.button_connect, &QPushButton::released,
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
            DesktopConfigDialog dialog(session_type,
                                       connect_data_.desktop_config,
                                       common::kSupportedVideoEncodings,
                                       this);

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
    net::Address address = net::Address::fromString(ui.edit_address->text());
    if (!address.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("An invalid computer address was entered."),
                             QMessageBox::Ok);
        ui.edit_address->setFocus();
    }
    else
    {
        proto::SessionType session_type = static_cast<proto::SessionType>(
            ui.combo_session_type->currentData().toInt());

        connect_data_.address = address.host();
        connect_data_.port = address.port();
        connect_data_.session_type = session_type;

        accept();
        close();
    }
}

} // namespace client
