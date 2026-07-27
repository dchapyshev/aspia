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

#ifndef CLIENT_DESKTOP_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H
#define CLIENT_DESKTOP_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H

#include <memory>

#include "client/desktop/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoDrives;
} // namespace Ui

namespace proto::system_info {
class PhysicalDrives;
} // namespace proto::system_info

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
    void onHealthContextMenu(const QPoint& point);

    // Fills the lower pane with the health data of the drive the upper pane has selected.
    void onCurrentDriveChanged();

private:
    void showContextMenu(QTreeWidget* tree, const QPoint& point);

    // Column sets of the lower pane. An ATA drive reports a table of normalized attributes, an NVMe
    // drive a set of named counters, so each gets the columns that fit it.
    void setAtaHealth(int drive_index);
    void setNvmeHealth(int drive_index);

    std::unique_ptr<Ui::SysInfoDrives> ui;

    // Drives of the last report. Kept because the lower pane is rebuilt on every selection change.
    std::unique_ptr<proto::system_info::PhysicalDrives> drives_;

    // Tree the context menu was opened on, so that the copy actions know what to copy.
    QTreeWidget* context_tree_ = nullptr;
};

#endif // CLIENT_DESKTOP_SYS_INFO_SYS_INFO_WIDGET_DRIVES_H
