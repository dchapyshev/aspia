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

#include "client/ui/sys_info/sys_info_widget_net_shares.h"

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

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetNetShares::SysInfoWidgetNetShares(QWidget* parent)
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
            this, &SysInfoWidgetNetShares::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetNetShares::~SysInfoWidgetNetShares() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetNetShares::category() const
{
    return common::kSystemInfo_NetworkShares;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetNetShares::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_network_shares())
    {
        LOG(INFO) << "No network shares";
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::NetworkShares& network_shares = system_info.network_shares();

    for (int i = 0; i < network_shares.share_size(); ++i)
    {
        const proto::system_info::NetworkShares::Share& share = network_shares.share(i);
        QList<QTreeWidgetItem*> group;

        if (!share.description().empty())
            group << mk(tr("Description"), QString::fromStdString(share.description()));

        if (!share.type().empty())
            group << mk(tr("Type"), QString::fromStdString(share.type()));

        if (!share.local_path().empty())
            group << mk(tr("Local Path"), QString::fromStdString(share.local_path()));

        group << mk(tr("Current Uses"), QString::number(share.current_uses()));

        QString max_uses = (share.max_uses() == 0xFFFFFFFF) ?
            tr("Not limited") : QString::number(share.max_uses());
        group << mk(tr("Maximum Uses"), max_uses);

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/folder-share.png",
                                              QString::fromStdString(share.name()),
                                              group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetNetShares::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetNetShares::onContextMenu(const QPoint& point)
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
