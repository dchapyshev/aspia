//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/desktop/update_server_dialog.h"

#include <QAbstractButton>
#include <QTimer>
#include <QUrl>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/time_types.h"
#include "client/system_settings.h"
#include "client/desktop/ui_update_server_dialog.h"
#include "common/desktop/msg_box.h"

namespace {

const int kPublicKeyLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateServerDialog::UpdateServerDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateServerDialog>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    SystemSettings settings;

    ui->edit_server->setPlaceholderText(kUpdateServer);
    ui->edit_server->setText(settings.updateServer());
    ui->edit_public_key->setMaxLength(kPublicKeyLength);
    ui->edit_public_key->setText(QString::fromLatin1(settings.updatePublicKey().toHex()));

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &UpdateServerDialog::onButtonBoxClicked);

    QTimer::singleShot(MilliSeconds(0), this, [this]() { setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
UpdateServerDialog::~UpdateServerDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateServerDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonbox->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    const QString server = ui->edit_server->text().trimmed();
    const QString public_key = ui->edit_public_key->text().trimmed();

    if (!server.isEmpty())
    {
        QUrl url(server, QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme() != "https" && url.scheme() != "http"))
        {
            MsgBox::warning(this, tr("An invalid update server address was entered."));
            ui->edit_server->setFocus();
            ui->edit_server->selectAll();
            return;
        }
    }
    else if (!public_key.isEmpty())
    {
        MsgBox::warning(this, tr("Enter the update server address."));
        ui->edit_server->setFocus();
        return;
    }

    const QByteArray public_key_hex = public_key.toLower().toLatin1();
    if (!public_key.isEmpty() && (public_key.size() != kPublicKeyLength ||
                                  QByteArray::fromHex(public_key_hex).toHex() != public_key_hex))
    {
        MsgBox::warning(this, tr("An invalid public key was entered."));
        ui->edit_public_key->setFocus();
        ui->edit_public_key->selectAll();
        return;
    }

    LOG(INFO) << "[ACTION] Update server changed";

    SystemSettings settings;
    settings.setUpdateServer(server);
    settings.setUpdatePublicKey(QByteArray::fromHex(public_key_hex));

    if (!settings.sync())
    {
        MsgBox::warning(this, tr("Failed to save the update server."));
        return;
    }

    accept();
}
