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

#include "client/desktop/management/host_telemetry_dialog.h"

#include <QPushButton>

#include "base/logging.h"
#include "client/router_controller.h"
#include "client/settings.h"
#include "client/telemetry_model.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_host_telemetry_dialog.h"

//--------------------------------------------------------------------------------------------------
HostTelemetryDialog::HostTelemetryDialog(qint64 router_id, const RouterHost& host, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::HostTelemetryDialog>()),
      router_id_(router_id),
      host_(host),
      model_(new TelemetryModel(this))
{
    LOG(TRACE) << "Ctor";
    ui->setupUi(this);

    ui->tree_telemetry->setModel(model_);
    ui->label_message->setVisible(false);

    Settings settings;
    restoreGeometry(settings.dialogGeometry(objectName()));

    connect(ui->button_refresh, &QPushButton::clicked, this, &HostTelemetryDialog::onRefresh);
    connect(ui->button_box, &QDialogButtonBox::rejected, this, &HostTelemetryDialog::reject);

    connect(&RouterController::instance(), &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
            reject();
    });

    requestTelemetry();
}

//--------------------------------------------------------------------------------------------------
HostTelemetryDialog::~HostTelemetryDialog()
{
    LOG(TRACE) << "Dtor";

    Settings settings;
    settings.setDialogGeometry(objectName(), saveGeometry());
}

//--------------------------------------------------------------------------------------------------
void HostTelemetryDialog::onRefresh()
{
    LOG(INFO) << "[ACTION] Telemetry refresh requested by user";
    requestTelemetry();
}

//--------------------------------------------------------------------------------------------------
void HostTelemetryDialog::onTelemetryReceived(const proto::router::HostTelemetryResult& result)
{
    ui->button_refresh->setEnabled(true);

    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the telemetry of host" << host_.host_id << ":" << error_code;
        MsgBox::warning(this, routerErrorText(error_code));
        return;
    }

    showTelemetry(QByteArray::fromStdString(result.json()));
}

//--------------------------------------------------------------------------------------------------
void HostTelemetryDialog::requestTelemetry()
{
    RouterSession* session = RouterController::session(router_id_);
    if (!session)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        return;
    }

    ui->button_refresh->setEnabled(false);

    session->requestHostTelemetry(host_.host_id, { this, &HostTelemetryDialog::onTelemetryReceived });
}

//--------------------------------------------------------------------------------------------------
void HostTelemetryDialog::showTelemetry(const QByteArray& json)
{
    QString message;

    switch (model_->setTelemetry(json))
    {
        case TelemetryModel::Status::OK:
            break;

        case TelemetryModel::Status::EMPTY:
            message = tr("The host has not reported telemetry yet.");
            break;

        case TelemetryModel::Status::INVALID:
            message = tr("The telemetry of the host could not be read.");
            break;

        case TelemetryModel::Status::UNSUPPORTED_VERSION:
            message = tr("The telemetry of the host is in a newer format. Update the client to view it.");
            break;
    }

    ui->label_message->setText(message);
    ui->label_message->setVisible(!message.isEmpty());
    ui->tree_telemetry->setVisible(message.isEmpty());

    for (int row = 0; row < model_->rowCount(); ++row)
        ui->tree_telemetry->setFirstColumnSpanned(row, QModelIndex(), true);

    ui->tree_telemetry->expandAll();
    ui->tree_telemetry->resizeColumnToContents(0);
}
