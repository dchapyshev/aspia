//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__SYS_INFO_DRIVERS_H
#define CLIENT__UI__SYS_INFO_DRIVERS_H

#include "client/ui/sys_info_widget.h"
#include "ui_sys_info_widget_drivers.h"

namespace client {

class SysInfoWidgetDrivers : public SysInfoWidget
{
    Q_OBJECT

public:
    SysInfoWidgetDrivers(QWidget* parent = nullptr);
    ~SysInfoWidgetDrivers();

    // SysInfo implementation.
    std::string category() const override;
    void setSystemInfo(const proto::SystemInfo& system_info) override;
    QTreeWidget* treeWidget() override;

private slots:
    void onContextMenu(const QPoint& point);

private:
    Ui::SysInfoDrivers ui;
};

} // namespace client

#endif // CLIENT__UI__SYS_INFO_DRIVERS_H
