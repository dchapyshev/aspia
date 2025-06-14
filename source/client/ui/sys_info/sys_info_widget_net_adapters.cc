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

#include "client/ui/sys_info/sys_info_widget_net_adapters.h"

#include <QMenu>

#include "base/logging.h"
#include "common/system_info_constants.h"

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
    Q_DISABLE_COPY(Item)
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
SysInfoWidgetNetAdapters::SysInfoWidgetNetAdapters(QWidget* parent)
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
            this, &SysInfoWidgetNetAdapters::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetNetAdapters::~SysInfoWidgetNetAdapters() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetNetAdapters::category() const
{
    return common::kSystemInfo_NetworkAdapters;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetNetAdapters::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_network_adapters())
    {
        LOG(INFO) << "No network adapters";
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::NetworkAdapters& adapters = system_info.network_adapters();

    for (int i = 0; i < adapters.adapter_size(); ++i)
    {
        const proto::system_info::NetworkAdapters::Adapter& adapter = adapters.adapter(i);
        QList<QTreeWidgetItem*> group;

        if (!adapter.adapter_name().empty())
            group << mk(tr("Adapter Name"), adapter.adapter_name());

        if (!adapter.iface().empty())
            group << mk(tr("Interface Type"), adapter.iface());

        if (adapter.speed())
            group << mk(tr("Connection Speed"), speedToString(adapter.speed()));

        if (!adapter.mac().empty())
            group << mk(tr("MAC Address"), adapter.mac());

        group << mk(tr("DHCP Enabled"), adapter.dhcp_enabled() ? tr("Yes") : tr("No"));

        for (int j = 0; j < adapter.dhcp_size(); ++j)
        {
            QString param = (adapter.dhcp_size() > 1) ?
                tr("DHCP Server #%1").arg(j + 1) : tr("DHCP Server");

            group << mk(param, adapter.dhcp(j));
        }

        for (int j = 0; j < adapter.address_size(); ++j)
        {
            QString value = QString("%1 / %2")
                .arg(QString::fromStdString(adapter.address(j).ip()),
                     QString::fromStdString(adapter.address(j).mask()));

            QString param = (adapter.address_size() > 1) ?
                tr("Address #%1").arg(j + 1) : tr("Address");

            group << mk(param, value);
        }

        for (int j = 0; j < adapter.gateway_size(); ++j)
        {
            QString param = (adapter.gateway_size() > 1) ?
                tr("Gateway #%1").arg(j + 1) : tr("Gateway");

            group << mk(param, adapter.gateway(j));
        }

        for (int j = 0; j < adapter.dns_size(); ++j)
        {
            QString param = (adapter.dns_size() > 1) ?
                QString("DNS #%1").arg(j + 1) : QString("DNS");

            group << mk(param, adapter.dns(j));
        }

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/graphic-card.png",
                                              QString::fromStdString(adapter.connection_name()),
                                              group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetNetAdapters::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetNetAdapters::onContextMenu(const QPoint& point)
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
