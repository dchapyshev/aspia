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

#include "client/ui/sys_info/sys_info_widget_processes.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

namespace {

class ProcessTreeItem final : public QTreeWidgetItem
{
public:
    ProcessTreeItem(const proto::system_info::Processes::Process& process)
        : process_(process)
    {
        // Nothing
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 1)
        {
            const ProcessTreeItem* other_item =
                static_cast<const ProcessTreeItem*>(&other);
            return process_.pid() < other_item->process_.pid();
        }
        else if (column == 2)
        {
            const ProcessTreeItem* other_item =
                static_cast<const ProcessTreeItem*>(&other);
            return process_.cpu() < other_item->process_.cpu();
        }
        else if (column == 3)
        {
            const ProcessTreeItem* other_item =
                static_cast<const ProcessTreeItem*>(&other);
            return process_.memory() < other_item->process_.memory();
        }
        else if (column == 4)
        {
            const ProcessTreeItem* other_item =
                static_cast<const ProcessTreeItem*>(&other);
            return process_.sid() < other_item->process_.sid();
        }

        return QTreeWidgetItem::operator<(other);
    }

private:
    proto::system_info::Processes::Process process_;
    Q_DISABLE_COPY(ProcessTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetProcesses::SysInfoWidgetProcesses(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 0);
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 1);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetProcesses::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetProcesses::~SysInfoWidgetProcesses() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetProcesses::category() const
{
    return common::kSystemInfo_Processes;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetProcesses::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_processes())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Processes& processes = system_info.processes();
    QIcon item_icon(":/img/heart-monitor.svg");

    for (int i = 0; i < processes.process_size(); ++i)
    {
        const proto::system_info::Processes::Process& process = processes.process(i);

        ProcessTreeItem* item = new ProcessTreeItem(process);
        item->setIcon(0, item_icon);

        QString process_name;
        if (process.name().empty())
        {
            if (process.pid() == 0)
                process_name = tr("System Idle Process");
            else
                process_name = tr("Unknown Process");
        }
        else
        {
            process_name = QString::fromStdString(process.name());
        }

        QString user_name;
        if (process.pid() == 0)
            user_name = "SYSTEM";
        else
            user_name = QString::fromStdString(process.user());

        item->setText(0, process_name);
        item->setText(1, QString::number(process.pid()));
        item->setText(2, QString("%1%").arg(process.cpu()));
        item->setText(3, sizeToString(process.memory()));
        item->setText(4, QString::number(process.sid()));
        item->setText(5, user_name);
        item->setText(6, QString::fromStdString(process.path()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 150);
    ui.tree->setColumnWidth(1, 50);
    ui.tree->setColumnWidth(2, 40);
    ui.tree->setColumnWidth(3, 70);
    ui.tree->setColumnWidth(4, 60);
    ui.tree->setColumnWidth(5, 110);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetProcesses::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetProcesses::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_name);
    menu.addAction(ui.action_copy_value);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
