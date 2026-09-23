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

#ifndef CLIENT_DESKTOP_MANAGEMENT_HOST_TELEMETRY_DIALOG_H
#define CLIENT_DESKTOP_MANAGEMENT_HOST_TELEMETRY_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/router_types.h"

namespace Ui {
class HostTelemetryDialog;
} // namespace Ui

namespace proto::router {
class HostTelemetryResult;
} // namespace proto::router

class TelemetryModel;

// Shows the telemetry the router keeps for a host.
class HostTelemetryDialog final : public QDialog
{
    Q_OBJECT

public:
    HostTelemetryDialog(qint64 router_id, const RouterHost& host, QWidget* parent);
    ~HostTelemetryDialog() final;

private slots:
    void onRefresh();
    void onTelemetryReceived(const proto::router::HostTelemetryResult& result);

private:
    void requestTelemetry();
    void showTelemetry(const QByteArray& json);

    std::unique_ptr<Ui::HostTelemetryDialog> ui;
    qint64 router_id_ = 0;
    RouterHost host_;
    TelemetryModel* model_ = nullptr;

    Q_DISABLE_COPY_MOVE(HostTelemetryDialog)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_HOST_TELEMETRY_DIALOG_H
