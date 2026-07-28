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

#ifndef COMMON_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H
#define COMMON_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H

#include <memory>

#include "common/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoDrives;
} // namespace Ui

class QTreeWidget;

class SysInfoWidgetDrives final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetDrives(QWidget* parent = nullptr);
    ~SysInfoWidgetDrives() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private slots:
    void onContextMenu(const QPoint& point);

private:
    std::unique_ptr<Ui::SysInfoDrives> ui;
};

#endif // COMMON_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H
