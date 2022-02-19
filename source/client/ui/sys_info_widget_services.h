//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__SYS_INFO_SERVICES_H
#define CLIENT__UI__SYS_INFO_SERVICES_H

#include "client/ui/sys_info_widget.h"
#include "ui_sys_info_widget_services.h"

namespace client {

class SysInfoWidgetServices : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetServices(QWidget* parent = nullptr);
    ~SysInfoWidgetServices() override;

    // SysInfo implementation.
    std::string category() const override;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) override;
    QTreeWidget* treeWidget() override;

private slots:
    void onContextMenu(const QPoint& point);

private:
    static QString statusToString(proto::system_info::Services::Service::Status status);
    static QString startupTypeToString(
        proto::system_info::Services::Service::StartupType startup_type);

    Ui::SysInfoServices ui;
    int current_column_ = 0;
};

} // namespace client

#endif // CLIENT__UI__SYS_INFO_SERVICES_H
