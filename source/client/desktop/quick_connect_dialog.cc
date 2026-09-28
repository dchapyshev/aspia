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

#include "client/desktop/quick_connect_dialog.h"

#include <QDataStream>
#include <QPushButton>
#include <QTimer>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/time_types.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/desktop/ui_quick_connect_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
QuickConnectDialog::QuickConnectDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::QuickConnectDialog>())
{
    LOG(TRACE) << "Ctor";

    ui->setupUi(this);

    QDataStream stream(Settings().quickConnectState());
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray geometry;
    qint64 router_id = 0;
    qint32 session_type = 0;
    stream >> geometry >> router_id >> session_type;

    // The router and the session type are selected once the lists are filled.
    if (stream.status() == QDataStream::Ok)
    {
        restoreGeometry(geometry);
        router_id_ = router_id;
        session_type_ = session_type;
    }

    QPushButton* connect_button = ui->button_box->addButton(tr("Connect"), QDialogButtonBox::AcceptRole);
    connect_button->setDefault(true);

    connect(ui->combo_router, &QComboBox::currentIndexChanged, this, &QuickConnectDialog::onRouterChanged);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &QuickConnectDialog::onButtonBoxClicked);

    ui->edit_address->setFocus();

    QTimer::singleShot(MilliSeconds::zero(), this, &QuickConnectDialog::onLoadData);
}

//--------------------------------------------------------------------------------------------------
QuickConnectDialog::~QuickConnectDialog()
{
    LOG(TRACE) << "Dtor";

    QByteArray state;

    {
        QDataStream stream(&state, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << saveGeometry();
        stream << ui->combo_router->currentData().toLongLong();
        stream << static_cast<qint32>(sessionType());
    }

    Settings().setQuickConnectState(state);
}

//--------------------------------------------------------------------------------------------------
HostConfig QuickConnectDialog::host() const
{
    const qint64 router_id = ui->combo_router->currentData().toLongLong();
    const QString address = ui->edit_address->text().trimmed();

    if (router_id != 0)
        return HostConfig::forRouterHost(router_id, stringToHostId(address), QString());

    HostConfig host;
    host.setAddress(address);
    return host;
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType QuickConnectDialog::sessionType() const
{
    return static_cast<proto::peer::SessionType>(ui->combo_session_type->currentData().toInt());
}

//--------------------------------------------------------------------------------------------------
void QuickConnectDialog::onRouterChanged(int /* index */)
{
    updateAddressLabel();
}

//--------------------------------------------------------------------------------------------------
void QuickConnectDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->buttonRole(button) != QDialogButtonBox::AcceptRole)
    {
        LOG(INFO) << "[ACTION] Quick connect rejected by user";
        reject();
        return;
    }

    const qint64 router_id = ui->combo_router->currentData().toLongLong();
    const QString address = ui->edit_address->text().trimmed();

    if (router_id == 0)
    {
        // Digits alone are an ID, and a host is found by its ID only through a router.
        if (isHostId(address))
        {
            MsgBox::warning(this, tr("Select a router to connect by ID."));
            ui->combo_router->setFocus();
            return;
        }

        if (!Address::fromString(address, kDefaultHostTcpPort).isValid())
        {
            MsgBox::warning(this, tr("An invalid host address was entered."));
            ui->edit_address->setFocus();
            ui->edit_address->selectAll();
            return;
        }
    }
    else if (!isHostId(address))
    {
        MsgBox::warning(this, tr("An invalid host ID was entered."));
        ui->edit_address->setFocus();
        ui->edit_address->selectAll();
        return;
    }

    LOG(INFO) << "[ACTION] Quick connect accepted by user";
    accept();
}

//--------------------------------------------------------------------------------------------------
void QuickConnectDialog::onLoadData()
{
    ui->combo_router->addItem(QIcon(":/img/connect.svg"), tr("Without Router"), QVariant::fromValue<qint64>(0));

    QList<RouterConfig> routers;
    const Database::ReadResult routers_result = Database::instance().routerList(&routers);
    if (routers_result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of routers";
        MsgBox::warning(this, tr("Failed to read the list of routers."));
        reject();
        return;
    }

    if (routers_result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the routers";

    const QIcon router_icon(":/img/stack.svg");

    for (const RouterConfig& router : std::as_const(routers))
        ui->combo_router->addItem(router_icon, router.displayLabel(), QVariant::fromValue(router.routerId()));

    // A router deleted since the dialog was closed leaves the choice without a router.
    const int router_index = ui->combo_router->findData(QVariant::fromValue(router_id_));
    if (router_index >= 0)
        ui->combo_router->setCurrentIndex(router_index);

    const proto::peer::SessionType session_types[] =
    {
        proto::peer::SESSION_TYPE_DESKTOP,
        proto::peer::SESSION_TYPE_TERMINAL,
        proto::peer::SESSION_TYPE_FILE_TRANSFER,
        proto::peer::SESSION_TYPE_CHAT,
        proto::peer::SESSION_TYPE_SYSTEM_INFO
    };

    for (proto::peer::SessionType type : session_types)
        ui->combo_session_type->addItem(sessionIcon(type), sessionName(type), QVariant::fromValue<int>(type));

    const int session_type_index = ui->combo_session_type->findData(QVariant::fromValue<int>(session_type_));
    if (session_type_index >= 0)
        ui->combo_session_type->setCurrentIndex(session_type_index);

    updateAddressLabel();
    setFixedHeight(sizeHint().height());
}

//--------------------------------------------------------------------------------------------------
void QuickConnectDialog::updateAddressLabel()
{
    if (ui->combo_router->currentData().toLongLong() == 0)
    {
        ui->label_address->setText(tr("Address:"));
        ui->edit_address->setPlaceholderText(tr("Host name or IP address"));
    }
    else
    {
        ui->label_address->setText(tr("ID:"));
        ui->edit_address->setPlaceholderText(tr("Host ID"));
    }
}
