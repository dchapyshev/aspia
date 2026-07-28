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

#ifndef COMMON_SYS_INFO_SYS_INFO_WIDGET_DMI_H
#define COMMON_SYS_INFO_SYS_INFO_WIDGET_DMI_H

#include <QStringList>

#include <memory>

#include "common/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoDmi;
} // namespace Ui

namespace proto::system_info {
class Dmi;
} // namespace proto::system_info

class QTreeWidget;

class SysInfoWidgetDmi final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetDmi(QWidget* parent = nullptr);
    ~SysInfoWidgetDmi() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private slots:
    void onContextMenu(const QPoint& point);
    void onParametersContextMenu(const QPoint& point);

    // Fills the lower pane with the parameters of the table the upper pane has selected.
    void onCurrentTableChanged();

private:
    // Adds a group of tables to the upper pane. A group of several tables gets an item per entry
    // of |entries|, a group of a single table stays a leaf.
    void addGroup(int group, const QString& icon_path, const QString& title,
                  const QStringList& entries);

    void showContextMenu(QTreeWidget* tree, const QPoint& point);

    QString biosTitle(int index) const;
    QList<QTreeWidgetItem*> biosParameters(int index) const;

    QString baseboardTitle(int index) const;
    QList<QTreeWidgetItem*> baseboardParameters(int index) const;

    QString chassisTitle(int index) const;
    QList<QTreeWidgetItem*> chassisParameters(int index) const;

    QString processorTitle(int index) const;
    QList<QTreeWidgetItem*> processorParameters(int index) const;

    std::unique_ptr<Ui::SysInfoDmi> ui;

    // Tables of the last report. Kept because the lower pane is rebuilt on every selection change.
    std::unique_ptr<proto::system_info::Dmi> dmi_;

    // Tree the context menu was opened on, so that the copy actions know what to copy.
    QTreeWidget* context_tree_ = nullptr;
};

#endif // COMMON_SYS_INFO_SYS_INFO_WIDGET_DMI_H
