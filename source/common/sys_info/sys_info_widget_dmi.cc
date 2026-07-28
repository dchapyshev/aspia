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

#include "common/sys_info/sys_info_widget_dmi.h"

#include <QMenu>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_dmi.h"

namespace {

const char kBiosIcon[] = ":/img/processor.svg";
const char kProcessorIcon[] = ":/img/microchip.svg";

// Group of tables a item of the upper pane belongs to.
constexpr int kGroupRole = Qt::UserRole;

// Index of the entry inside its group. The item standing for the group itself carries -1.
constexpr int kIndexRole = Qt::UserRole + 1;

enum Group
{
    GROUP_BIOS = 0,
    GROUP_PROCESSORS
};

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
                child->child(i)->setIcon(0, icon);
        }

        addChildren(childs);
    }

    // Group of parameters inside the parameters of a single table.
    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

private:
    Q_DISABLE_COPY_MOVE(Item)
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
SysInfoWidgetDmi::SysInfoWidgetDmi(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoDmi>())
{
    ui->setupUi(this);

    QList<int> sizes;
    sizes.emplace_back(250);
    sizes.emplace_back(250);
    ui->splitter->setSizes(sizes);

    connect(ui->action_copy_row, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyRow(context_tree_->currentItem());
    });

    connect(ui->action_copy_name, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyColumn(context_tree_->currentItem(), 0);
    });

    connect(ui->action_copy_value, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyColumn(context_tree_->currentItem(), 1);
    });

    connect(ui->tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetDmi::onContextMenu);
    connect(ui->tree_params, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetDmi::onParametersContextMenu);

    connect(ui->tree, &QTreeWidget::currentItemChanged,
            this, &SysInfoWidgetDmi::onCurrentTableChanged);

    connect(ui->tree_params, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDmi::~SysInfoWidgetDmi() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetDmi::category() const
{
    return kSystemInfo_Dmi;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui->tree->clear();
    ui->tree_params->clear();
    dmi_.reset();

    // Firmware without the tables at all, or one the reader was not allowed to read, arrives as a
    // report without them. The next report may still bring them.
    const bool has_dmi = system_info.has_dmi();

    ui->tree->setEnabled(has_dmi);
    ui->tree_params->setEnabled(has_dmi);

    if (!has_dmi)
        return;

    dmi_ = std::make_unique<proto::system_info::Dmi>(system_info.dmi());

    if (dmi_->bios_size())
    {
        // The specification allows a single BIOS table, so the group has nothing to enumerate.
        QTreeWidgetItem* group = new Item(kBiosIcon, tr("BIOS"), {});
        group->setData(0, kGroupRole, GROUP_BIOS);
        group->setData(0, kIndexRole, -1);

        ui->tree->addTopLevelItem(group);
    }

    if (dmi_->processor_size())
    {
        QList<QTreeWidgetItem*> childs;

        for (int i = 0; i < dmi_->processor_size(); ++i)
        {
            QTreeWidgetItem* child = new QTreeWidgetItem();

            child->setText(0, processorTitle(i));
            child->setData(0, kGroupRole, GROUP_PROCESSORS);
            child->setData(0, kIndexRole, i);

            childs << child;
        }

        QTreeWidgetItem* group = new Item(kProcessorIcon, tr("Processors"), childs);
        group->setData(0, kGroupRole, GROUP_PROCESSORS);
        group->setData(0, kIndexRole, -1);

        ui->tree->addTopLevelItem(group);
        group->setExpanded(true);
    }

    if (!isStateRestored())
        ui->tree->resizeColumnToContents(0);

    // Selecting the first table fills the lower pane.
    if (ui->tree->topLevelItemCount())
        ui->tree->setCurrentItem(ui->tree->topLevelItem(0));
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDmi::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onParametersContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree_params, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onCurrentTableChanged()
{
    ui->tree_params->clear();

    QTreeWidgetItem* current = ui->tree->currentItem();
    if (!current || !dmi_)
        return;

    const int group = current->data(0, kGroupRole).toInt();
    const int index = current->data(0, kIndexRole).toInt();

    QList<QTreeWidgetItem*> items;

    switch (group)
    {
        case GROUP_BIOS:
        {
            for (int i = 0; i < dmi_->bios_size(); ++i)
                items << new Item(kBiosIcon, biosTitle(i), biosParameters(i));
        }
        break;

        case GROUP_PROCESSORS:
        {
            // The item of the group shows every entry it holds, the item of an entry only the
            // parameters of that entry.
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->processor_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->processor_size(); ++i)
                items << new Item(kProcessorIcon, processorTitle(i), processorParameters(i));
        }
        break;

        default:
            break;
    }

    ui->tree_params->addTopLevelItems(items);

    // Groups of parameters are nested, and there is nothing to hide behind a collapsed item.
    ui->tree_params->expandAll();

    for (int i = 0; i < ui->tree_params->columnCount(); ++i)
        ui->tree_params->resizeColumnToContents(i);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::showContextMenu(QTreeWidget* tree, const QPoint& point)
{
    QTreeWidgetItem* current_item = tree->itemAt(point);
    if (!current_item)
        return;

    tree->setCurrentItem(current_item);
    context_tree_ = tree;

    QMenu menu;
    menu.addAction(ui->action_copy_row);
    menu.addAction(ui->action_copy_name);
    menu.addAction(ui->action_copy_value);

    menu.exec(tree->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::biosTitle(int index) const
{
    const proto::system_info::Dmi::Bios& bios = dmi_->bios(index);

    if (!bios.vendor().empty())
        return QString::fromStdString(bios.vendor());

    return tr("BIOS");
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::biosParameters(int index) const
{
    const proto::system_info::Dmi::Bios& bios = dmi_->bios(index);
    QList<QTreeWidgetItem*> items;

    if (!bios.vendor().empty())
        items << mk(tr("Vendor"), QString::fromStdString(bios.vendor()));

    if (!bios.version().empty())
        items << mk(tr("Version"), QString::fromStdString(bios.version()));

    if (!bios.release_date().empty())
        items << mk(tr("Release Date"), QString::fromStdString(bios.release_date()));

    if (bios.address())
    {
        items << mk(tr("Address"),
                    QString("0x%1").arg(QString::number(bios.address(), 16).toUpper()));
    }

    if (bios.rom_size())
        items << mk(tr("ROM Size"), Formatter::sizeToString(bios.rom_size()));

    if (!bios.revision().empty())
        items << mk(tr("Revision"), QString::fromStdString(bios.revision()));

    if (!bios.firmware_revision().empty())
    {
        items << mk(tr("Firmware Revision"),
                    QString::fromStdString(bios.firmware_revision()));
    }

    QList<QTreeWidgetItem*> characteristics;

    for (int i = 0; i < bios.characteristic_size(); ++i)
        characteristics << mk(QString::fromStdString(bios.characteristic(i)), QString());

    if (!characteristics.isEmpty())
        items << new Item(tr("Characteristics"), characteristics);

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::processorTitle(int index) const
{
    const proto::system_info::Dmi::Processor& processor = dmi_->processor(index);

    if (!processor.version().empty())
        return QString::fromStdString(processor.version());

    if (!processor.socket_designation().empty())
        return QString::fromStdString(processor.socket_designation());

    return tr("Processor %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::processorParameters(int index) const
{
    const proto::system_info::Dmi::Processor& processor = dmi_->processor(index);
    QList<QTreeWidgetItem*> items;

    if (!processor.populated())
    {
        items << mk(tr("Installed"), tr("No"));
        return items;
    }

    if (!processor.manufacturer().empty())
        items << mk(tr("Manufacturer"), QString::fromStdString(processor.manufacturer()));

    if (!processor.version().empty())
        items << mk(tr("Version"), QString::fromStdString(processor.version()));

    if (!processor.family().empty())
        items << mk(tr("Family"), QString::fromStdString(processor.family()));

    if (!processor.type().empty())
        items << mk(tr("Type"), QString::fromStdString(processor.type()));

    if (!processor.status().empty())
        items << mk(tr("Status"), QString::fromStdString(processor.status()));

    if (!processor.socket_designation().empty())
    {
        items << mk(tr("Socket Designation"),
                    QString::fromStdString(processor.socket_designation()));
    }

    if (!processor.socket().empty())
        items << mk(tr("Socket"), QString::fromStdString(processor.socket()));

    if (!processor.socket_type().empty())
        items << mk(tr("Socket Type"), QString::fromStdString(processor.socket_type()));

    if (processor.id())
        items << mk(tr("ID"), QString("%1").arg(processor.id(), 16, 16, QChar('0')).toUpper());

    if (processor.voltage() > 0.0)
        items << mk(tr("Voltage"), tr("%1 V").arg(processor.voltage(), 0, 'f', 1));

    if (processor.external_clock())
        items << mk(tr("External Clock"), tr("%1 MHz").arg(processor.external_clock()));

    if (processor.max_speed())
        items << mk(tr("Max Speed"), tr("%1 MHz").arg(processor.max_speed()));

    if (processor.current_speed())
        items << mk(tr("Current Speed"), tr("%1 MHz").arg(processor.current_speed()));

    if (processor.core_count())
        items << mk(tr("Core Count"), QString::number(processor.core_count()));

    if (processor.core_enabled())
        items << mk(tr("Cores Enabled"), QString::number(processor.core_enabled()));

    if (processor.thread_count())
        items << mk(tr("Thread Count"), QString::number(processor.thread_count()));

    if (processor.thread_enabled())
        items << mk(tr("Threads Enabled"), QString::number(processor.thread_enabled()));

    if (!processor.serial_number().empty())
        items << mk(tr("Serial Number"), QString::fromStdString(processor.serial_number()));

    if (!processor.asset_tag().empty())
        items << mk(tr("Asset Tag"), QString::fromStdString(processor.asset_tag()));

    if (!processor.part_number().empty())
        items << mk(tr("Part Number"), QString::fromStdString(processor.part_number()));

    items << mk(tr("64-bit Capable"), processor.support_64bit() ? tr("Yes") : tr("No"));
    items << mk(tr("Multi-Core"), processor.support_multi_core() ? tr("Yes") : tr("No"));
    items << mk(tr("Hardware Thread"), processor.support_hardware_thread() ? tr("Yes") : tr("No"));

    items << mk(tr("Execute Protection"),
                processor.support_execute_protection() ? tr("Yes") : tr("No"));
    items << mk(tr("Enhanced Virtualization"),
                processor.support_enhanced_virtualization() ? tr("Yes") : tr("No"));
    items << mk(tr("Power/Performance Control"),
                processor.support_power_control() ? tr("Yes") : tr("No"));

    return items;
}
