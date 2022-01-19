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

#ifndef CLIENT__UI__SYS_INFO_DEVICES_H
#define CLIENT__UI__SYS_INFO_DEVICES_H

#include "client/ui/sys_info_widget.h"
#include "ui_sys_info_widget_devices.h"

namespace client {

class SysInfoWidgetDevices : public SysInfoWidget
{
    Q_OBJECT

public:
    SysInfoWidgetDevices(QWidget* parent = nullptr);
    ~SysInfoWidgetDevices();

    // SysInfo implementation.
    std::string category() const override;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) override;
    QTreeWidget* treeWidget() override;

private slots:
    void onContextMenu(const QPoint& point);

private:
    Ui::SysInfoDevices ui;
    int current_column_ = 0;
};

} // namespace client

#endif // CLIENT__UI__SYS_INFO_DEVICES_H
