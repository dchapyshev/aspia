//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_SYS_INFO_SYS_INFO_POWER_OPTIONS_H
#define CLIENT_UI_SYS_INFO_SYS_INFO_POWER_OPTIONS_H

#include "client/ui/sys_info/sys_info_widget.h"
#include "ui_sys_info_widget_power_options.h"

namespace client {

class SysInfoWidgetPowerOptions final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetPowerOptions(QWidget* parent = nullptr);
    ~SysInfoWidgetPowerOptions() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private slots:
    void onContextMenu(const QPoint& point);
    static QString powerSourceToString(proto::system_info::PowerOptions::PowerSource value);
    static QString batteryStatusToString(proto::system_info::PowerOptions::BatteryStatus value);

private:
    Ui::SysInfoPowerOptions ui;
};

} // namespace client

#endif // CLIENT_UI_SYS_INFO_SYS_INFO_POWER_OPTIONS_H
