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

#include "client/ui/client_dialog.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "client/config_factory.h"
#include "client/ui/client_settings.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/ui/qt_desktop_window.h"
#include "client/ui/qt_file_manager_window.h"
#include "common/desktop_session_constants.h"
#include "common/session_type.h"
#include "net/address.h"
#include "ui_client_dialog.h"

#include <QMessageBox>

namespace client {

ClientDialog::ClientDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::ClientDialog>())
{
    config_.port = DEFAULT_HOST_TCP_PORT;
    config_.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
    desktop_config_ = ConfigFactory::defaultDesktopManageConfig();

    ui->setupUi(this);
    setFixedHeight(sizeHint().height());

    ClientSettings settings;
    ui->combo_address->addItems(settings.addressList());
    ui->combo_address->setCurrentIndex(0);

    auto add_session = [this](const QString& icon, proto::SessionType session_type)
    {
        ui->combo_session_type->addItem(QIcon(icon),
                                        common::sessionTypeToLocalizedString(session_type),
                                        QVariant(session_type));
    };

    add_session(QStringLiteral(":/img/monitor-keyboard.png"), proto::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(QStringLiteral(":/img/monitor.png"), proto::SESSION_TYPE_DESKTOP_VIEW);
    add_session(QStringLiteral(":/img/folder-stand.png"), proto::SESSION_TYPE_FILE_TRANSFER);

    int current_session_type = ui->combo_session_type->findData(QVariant(config_.session_type));
    if (current_session_type != -1)
    {
        ui->combo_session_type->setCurrentIndex(current_session_type);
        sessionTypeChanged(current_session_type);
    }

    connect(ui->button_clear, &QPushButton::released, [this]()
    {
        int ret = QMessageBox::question(
            this,
            tr("Confirmation"),
            tr("The list of entered addresses will be cleared. Continue?"),
            QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes)
        {
            ui->combo_address->clear();

            ClientSettings settings;
            settings.setAddressList(QStringList());
        }
    });

    connect(ui->combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientDialog::sessionTypeChanged);

    connect(ui->button_session_config, &QPushButton::released,
            this, &ClientDialog::sessionConfigButtonPressed);

    connect(ui->button_connect, &QPushButton::released,
            this, &ClientDialog::connectButtonPressed);

    ui->combo_address->setFocus();
}

ClientDialog::~ClientDialog() = default;

void ClientDialog::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui->combo_session_type->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            ui->button_session_config->setEnabled(true);
            desktop_config_ = ConfigFactory::defaultDesktopManageConfig();
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            ui->button_session_config->setEnabled(true);
            desktop_config_ = ConfigFactory::defaultDesktopViewConfig();
        }
        break;

        default:
            ui->button_session_config->setEnabled(false);
            break;
    }
}

void ClientDialog::sessionConfigButtonPressed()
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui->combo_session_type->currentData().toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            DesktopConfigDialog dialog(session_type,
                                       desktop_config_,
                                       common::kSupportedVideoEncodings,
                                       this);

            if (dialog.exec() == DesktopConfigDialog::Accepted)
                desktop_config_ = dialog.config();
        }
        break;

        default:
            break;
    }
}

void ClientDialog::connectButtonPressed()
{
    QString current_address = ui->combo_address->currentText();

    net::Address address = net::Address::fromString(current_address.toStdU16String());
    if (!address.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("An invalid computer address was entered."),
                             QMessageBox::Ok);
        ui->combo_address->setFocus();
    }
    else
    {
        int current_index = ui->combo_address->findText(current_address);
        if (current_index != -1)
            ui->combo_address->removeItem(current_index);

        ui->combo_address->insertItem(0, current_address);
        ui->combo_address->setCurrentIndex(0);

        QStringList address_list;
        for (int i = 0; i < std::min(ui->combo_address->count(), 15); ++i)
            address_list.append(ui->combo_address->itemText(i));

        ClientSettings settings;
        settings.setAddressList(address_list);

        proto::SessionType session_type = static_cast<proto::SessionType>(
            ui->combo_session_type->currentData().toInt());

        config_.address = address.host();
        config_.port = address.port();
        config_.session_type = session_type;

        ClientWindow* client_window = nullptr;

        switch (config_.session_type)
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
            case proto::SESSION_TYPE_DESKTOP_VIEW:
            {
                client_window = new QtDesktopWindow(
                    config_.session_type, desktop_config_, parentWidget());
            }
            break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                client_window = new client::QtFileManagerWindow(parentWidget());
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!client_window)
            return;

        client_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!client_window->connectToHost(config_))
        {
            client_window->close();
        }
        else
        {
            accept();
            close();
        }
    }
}

} // namespace client
