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

#include "client/ui/sys_info/sys_info_widget_applications.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetApplications::SysInfoWidgetApplications(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);
    ui.tree->setMouseTracking(true);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), current_column_);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetApplications::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });

    connect(ui.tree, &QTreeWidget::itemEntered,
            this, [this](QTreeWidgetItem* /* item */, int column)
    {
        current_column_ = column;
    });

    connect(ui.action_search_in_google, &QAction::triggered, this, [this]()
    {
        QTreeWidgetItem* item = ui.tree->currentItem();
        if (!item)
            return;

        QString name = item->text(0);
        if (name.isEmpty())
            return;

        searchInGoogle(name);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetApplications::~SysInfoWidgetApplications() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetApplications::category() const
{
    return common::kSystemInfo_Applications;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetApplications::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_applications())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Applications& applications = system_info.applications();
    QIcon item_icon(":/img/software.svg");

    for (int i = 0; i < applications.application_size(); ++i)
    {
        const proto::system_info::Applications::Application& application =
            applications.application(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(application.name()));
        item->setText(1, QString::fromStdString(application.version()));
        item->setText(2, QString::fromStdString(application.publisher()));
        item->setText(3, QString::fromStdString(application.install_date()));
        item->setText(4, QString::fromStdString(application.install_location()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 250);
    ui.tree->setColumnWidth(1, 100);
    ui.tree->setColumnWidth(2, 170);
    ui.tree->setColumnWidth(3, 80);
    ui.tree->setColumnWidth(4, 100);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetApplications::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetApplications::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_value);
    menu.addSeparator();
    menu.addAction(ui.action_search_in_google);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
