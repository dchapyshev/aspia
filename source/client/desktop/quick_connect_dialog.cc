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

#include <QLayout>
#include <QPushButton>
#include <QTimer>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/time_types.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/desktop/ui_quick_connect_dialog.h"
#include "client/storage/user_settings.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"

namespace {

const char kGeometryKey[] = "geometry";
const char kSessionTypeKey[] = "session_type";

} // namespace

//--------------------------------------------------------------------------------------------------
QuickConnectDialog::QuickConnectDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::QuickConnectDialog>())
{
    LOG(TRACE) << "Ctor";

    ui->setupUi(this);

    const QVariantMap state = UserSettings().quickConnectState();
    restoreGeometry(state.value(kGeometryKey).toByteArray());

    // The session type is selected once the list is filled.
    session_type_ = state.value(kSessionTypeKey, session_type_).toInt();

    QPushButton* connect_button = ui->button_box->addButton(tr("Connect"), QDialogButtonBox::AcceptRole);
    connect_button->setDefault(true);

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &QuickConnectDialog::onButtonBoxClicked);

    ui->edit_address->setFocus();

    QTimer::singleShot(MilliSeconds::zero(), this, &QuickConnectDialog::onLoadData);
}

//--------------------------------------------------------------------------------------------------
QuickConnectDialog::~QuickConnectDialog()
{
    LOG(TRACE) << "Dtor";

    QVariantMap state;
    state.insert(kGeometryKey, saveGeometry());
    state.insert(kSessionTypeKey, static_cast<qint32>(sessionType()));

    UserSettings().setQuickConnectState(state);
}

//--------------------------------------------------------------------------------------------------
HostConfig QuickConnectDialog::host() const
{
    const QString address = ui->edit_address->text().trimmed();

    if (isHostId(address))
        return HostConfig::forRouterHost(stringToHostId(address), QString());

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
void QuickConnectDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->buttonRole(button) != QDialogButtonBox::AcceptRole)
    {
        LOG(INFO) << "[ACTION] Quick connect rejected by user";
        reject();
        return;
    }

    const QString address = ui->edit_address->text().trimmed();

    // Digits alone are the ID of a host reached through the router.
    if (!isHostId(address) && !Address::fromString(address, kDefaultHostTcpPort).isValid())
    {
        MsgBox::warning(this, tr("An invalid host address was entered."));
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

    // The combo boxes only compute their content-based size hint once the layout has been activated for
    // the populated items; without this the dialog size hint uses the empty combo heights and fixing the
    // height to it squeezes the fields below their natural size.
    layout()->activate();
    setFixedHeight(sizeHint().height());
}
