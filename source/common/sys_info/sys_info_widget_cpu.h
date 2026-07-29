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

#ifndef COMMON_SYS_INFO_SYS_INFO_WIDGET_CPU_H
#define COMMON_SYS_INFO_SYS_INFO_WIDGET_CPU_H

#include <memory>

#include "common/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoCpu;
} // namespace Ui

namespace proto::system_info {
class Processor;
} // namespace proto::system_info

class QTreeWidget;
class QTreeWidgetItem;

class SysInfoWidgetCpu final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetCpu(QWidget* parent = nullptr);
    ~SysInfoWidgetCpu() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private slots:
    void onContextMenu(const QPoint& point);

private:
    // Groups of the tree, each built from what the host reported about the processor.
    QList<QTreeWidgetItem*> identityParameters(const proto::system_info::Processor& cpu) const;
    QList<QTreeWidgetItem*> cacheParameters(const proto::system_info::Processor& cpu) const;
    QList<QTreeWidgetItem*> featureParameters(const proto::system_info::Processor& cpu, int group) const;

    std::unique_ptr<Ui::SysInfoCpu> ui;
};

#endif // COMMON_SYS_INFO_SYS_INFO_WIDGET_CPU_H
