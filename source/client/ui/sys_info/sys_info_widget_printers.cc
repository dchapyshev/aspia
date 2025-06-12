//
// Aspia Project
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

#include "client/ui/sys_info/sys_info_widget_printers.h"

#include "base/macros_magic.h"
#include "base/logging.h"
#include "common/system_info_constants.h"

#include <QMenu>

namespace client {

namespace {

class Item : public QTreeWidgetItem
{
public:
    Item(const char* icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        for (const auto& child : childs)
        {
            child->setIcon(0, icon);

            for (int i = 0; i < child->childCount(); ++i)
            {
                QTreeWidgetItem* item = child->child(i);
                if (item)
                    item->setIcon(0, icon);
            }
        }

        addChildren(childs);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(Item);
};

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const QString& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);

    return item;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const std::string& value)
{
    return mk(param, QString::fromStdString(value));
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetPrinters::SysInfoWidgetPrinters(QWidget* parent)
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
            this, &SysInfoWidgetPrinters::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetPrinters::~SysInfoWidgetPrinters() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetPrinters::category() const
{
    return common::kSystemInfo_Printers;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetPrinters::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_printers())
    {
        LOG(LS_INFO) << "No printers";
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Printers& printers = system_info.printers();

    for (int i = 0; i < printers.printer_size(); ++i)
    {
        const proto::system_info::Printers::Printer& printer = printers.printer(i);
        QList<QTreeWidgetItem*> group;

        group << mk(tr("Default"), printer.is_default() ? tr("Yes") : tr("No"));

        if (!printer.port().empty())
            group << mk(tr("Port"), printer.port());

        if (!printer.driver().empty())
            group << mk(tr("Driver"), printer.driver());

        group << mk(tr("Shared"), printer.is_shared() ? tr("Yes") : tr("No"));

        if (printer.is_shared() && !printer.share_name().empty())
            group << mk(tr("Share Name"), printer.share_name());

        group << mk(tr("Jobs Count"), QString::number(printer.jobs_count()));

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/printer.png",
                                              QString::fromStdString(printer.name()),
                                              group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetPrinters::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetPrinters::onContextMenu(const QPoint& point)
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
