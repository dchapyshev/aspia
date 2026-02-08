//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/sys_info/sys_info_widget_env_vars.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetEnvVars::SysInfoWidgetEnvVars(QWidget* parent)
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
            this, &SysInfoWidgetEnvVars::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetEnvVars::~SysInfoWidgetEnvVars() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetEnvVars::category() const
{
    return common::kSystemInfo_EnvironmentVariables;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetEnvVars::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_env_vars())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::EnvironmentVariables& variables = system_info.env_vars();
    QIcon item_icon(":/img/day-view.svg");

    for (int i = 0; i < variables.variable_size(); ++i)
    {
        const proto::system_info::EnvironmentVariables::Variable& variable = variables.variable(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(variable.name()));
        item->setText(1, QString::fromStdString(variable.value()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetEnvVars::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetEnvVars::onContextMenu(const QPoint& point)
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
