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

#include "client/ui/sys_info/sys_info_widget_video_adapters.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

namespace {

class Item : public QTreeWidgetItem
{
public:
    Item(const QString& icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
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

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetVideoAdapters::SysInfoWidgetVideoAdapters(QWidget* parent)
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
            this, &SysInfoWidgetVideoAdapters::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetVideoAdapters::~SysInfoWidgetVideoAdapters() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetVideoAdapters::category() const
{
    return common::kSystemInfo_VideoAdapters;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetVideoAdapters::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_video_adapters())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::VideoAdapters& video_adapters = system_info.video_adapters();

    for (int i = 0; i < video_adapters.adapter_size(); ++i)
    {
        const proto::system_info::VideoAdapters::Adapter& adapter = video_adapters.adapter(i);
        QList<QTreeWidgetItem*> group;

        if (!adapter.description().empty())
            group << mk(tr("Description"), QString::fromStdString(adapter.description()));

        if (!adapter.adapter_string().empty())
            group << mk(tr("Adapter String"), QString::fromStdString(adapter.adapter_string()));

        if (!adapter.bios_string().empty())
            group << mk(tr("BIOS String"), QString::fromStdString(adapter.bios_string()));

        if (!adapter.chip_type().empty())
            group << mk(tr("Chip Type"), QString::fromStdString(adapter.chip_type()));

        if (!adapter.dac_type().empty())
            group << mk(tr("DAC Type"), QString::fromStdString(adapter.dac_type()));

        if (adapter.memory_size() != 0)
            group << mk(tr("Memory Size"), tr("%1 bytes").arg(adapter.memory_size()));

        if (!adapter.driver_date().empty())
            group << mk(tr("Driver Date"), QString::fromStdString(adapter.driver_date()));

        if (!adapter.driver_version().empty())
            group << mk(tr("Driver Version"), QString::fromStdString(adapter.driver_version()));

        if (!adapter.driver_provider().empty())
            group << mk(tr("Driver Provider"), QString::fromStdString(adapter.driver_provider()));

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/video-card.svg", QString::fromStdString(adapter.description()), group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetVideoAdapters::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetVideoAdapters::onContextMenu(const QPoint& point)
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
