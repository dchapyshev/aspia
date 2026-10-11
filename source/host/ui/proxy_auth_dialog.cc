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

#include "host/ui/proxy_auth_dialog.h"

#include <QAbstractButton>
#include <QTimer>

#include "base/logging.h"
#include "base/time_types.h"
#include "common/desktop/password_edit.h"
#include "host/ui/ui_proxy_auth_dialog.h"

//--------------------------------------------------------------------------------------------------
ProxyAuthDialog::ProxyAuthDialog(const ProxySettings& proxy, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::ProxyAuthDialog>()),
      proxy_(proxy)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->label_description->setText(
        tr("The system settings specify the proxy server %1:%2. Enter the user name and password for it, "
           "or leave them empty if it does not require them. Cancel to connect without the proxy server.")
            .arg(proxy_.address).arg(proxy_.port));

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &ProxyAuthDialog::onButtonBoxClicked);

    QTimer::singleShot(MilliSeconds(0), this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
ProxyAuthDialog::~ProxyAuthDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
ProxySettings ProxyAuthDialog::proxy() const
{
    ProxySettings proxy = proxy_;
    proxy.user = ui->edit_username->text();
    proxy.password = ui->edit_password->password();
    return proxy;
}

//--------------------------------------------------------------------------------------------------
void ProxyAuthDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
        return;
    }

    LOG(INFO) << "[ACTION] Accepted by user";
    accept();
}
