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

#ifndef COMMON_SYS_INFO_SYS_INFO_WIDGET_SMART_H
#define COMMON_SYS_INFO_SYS_INFO_WIDGET_SMART_H

#include <QStringList>

#include <memory>

#include "common/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoSmart;
} // namespace Ui

namespace proto::system_info {
class PhysicalDrives;
} // namespace proto::system_info

class QTreeWidget;
class QTreeWidgetItem;

class SysInfoWidgetSmart final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetSmart(QWidget* parent = nullptr);
    ~SysInfoWidgetSmart() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

    // The pane shows the health of one drive at a time, the report holds the health of all of them.
    void buildReport(SysInfoReport* report) final;

private slots:
    void onContextMenu(const QPoint& point);
    void onHealthContextMenu(const QPoint& point);

    // Fills the lower pane with the health data of the drive the upper pane has selected.
    void onCurrentDriveChanged();

private:
    void showContextMenu(QTreeWidget* tree, const QPoint& point);

    // Name the drive is listed under.
    QString driveTitle(int drive_index) const;

    // Health data of a drive. An ATA drive reports a table of normalized attributes, an NVMe drive
    // a set of named counters, and a drive that reports neither has a single row saying so, so each
    // of them comes with the columns that fit it.
    QStringList healthHeader(int drive_index) const;
    QList<QTreeWidgetItem*> healthItems(int drive_index) const;

    QList<QTreeWidgetItem*> ataHealth(int drive_index) const;
    QList<QTreeWidgetItem*> nvmeHealth(int drive_index) const;

    std::unique_ptr<Ui::SysInfoSmart> ui;

    // Drives of the last report. Kept because the lower pane is rebuilt on every selection change.
    std::unique_ptr<proto::system_info::PhysicalDrives> drives_;

    // Tree the context menu was opened on, so that the copy actions know what to copy.
    QTreeWidget* context_tree_ = nullptr;
};

#endif // COMMON_SYS_INFO_SYS_INFO_WIDGET_SMART_H
