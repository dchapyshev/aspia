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

#include "client/ui/desktop_config_dialog.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_desktop_view.h"
#include "client/config_factory.h"
#include "ui_client_dialog.h"

namespace aspia {

ClientDialog::ClientDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ClientDialog()),
      computer_(ConfigFactory::defaultComputer())
{
    ui->setupUi(this);
    setFixedHeight(sizeHint().height());

    ui->edit_address->setText(QString::fromStdString(computer_.address()));
    ui->spin_port->setValue(computer_.port());

    QComboBox* combo_session_type = ui->combo_session_type;
    combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor-keyboard.png")),
                                    tr("Desktop Manage"),
                                    QVariant(proto::SESSION_TYPE_DESKTOP_MANAGE));

    combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor.png")),
                                    tr("Desktop View"),
                                    QVariant(proto::SESSION_TYPE_DESKTOP_VIEW));

    combo_session_type->addItem(QIcon(QStringLiteral(":/icon/folder-stand.png")),
                                    tr("File Transfer"),
                                    QVariant(proto::SESSION_TYPE_FILE_TRANSFER));

    int current_session_type =
        combo_session_type->findData(QVariant(computer_.session_type()));
    if (current_session_type != -1)
    {
        combo_session_type->setCurrentIndex(current_session_type);
        sessionTypeChanged(current_session_type);
    }

    connect(combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientDialog::sessionTypeChanged);

    connect(ui->button_session_config, &QPushButton::pressed,
            this, &ClientDialog::sessionConfigButtonPressed);

    connect(ui->button_connect, &QPushButton::pressed,
            this, &ClientDialog::connectButtonPressed);

    ui->edit_address->setFocus();
}

ClientDialog::~ClientDialog() = default;

void ClientDialog::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui->combo_session_type->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui->button_session_config->setEnabled(true);
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
        {
            DesktopConfigDialog dialog(session_type,
                                       computer_.session_config().desktop_manage(),
                                       this);

            if (dialog.exec() == DesktopConfigDialog::Accepted)
            {
                computer_.mutable_session_config()->mutable_desktop_manage()->CopyFrom(
                    dialog.config());
            }
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            DesktopConfigDialog dialog(session_type,
                                       computer_.session_config().desktop_view(),
                                       this);
            if (dialog.exec() == DesktopConfigDialog::Accepted)
            {
                computer_.mutable_session_config()->mutable_desktop_view()->CopyFrom(
                    dialog.config());
            }
        }
        break;

        default:
            break;
    }
}

void ClientDialog::connectButtonPressed()
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui->combo_session_type->currentData().toInt());

    computer_.set_address(ui->edit_address->text().toStdString());
    computer_.set_port(ui->spin_port->value());
    computer_.set_session_type(session_type);

    accept();
    close();
}

} // namespace aspia
