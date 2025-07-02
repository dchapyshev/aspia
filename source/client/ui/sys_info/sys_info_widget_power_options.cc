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

#include "client/ui/sys_info/sys_info_widget_power_options.h"

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

    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

    Item(const QIcon& icon, const QString& param, const QString& value)
    {
        setIcon(0, icon);
        setText(0, param);
        setText(1, value);
    }

private:
    Q_DISABLE_COPY(Item);
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
SysInfoWidgetPowerOptions::SysInfoWidgetPowerOptions(QWidget* parent)
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
            this, &SysInfoWidgetPowerOptions::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetPowerOptions::~SysInfoWidgetPowerOptions() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetPowerOptions::category() const
{
    return common::kSystemInfo_PowerOptions;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetPowerOptions::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_power_options())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::PowerOptions& power_options = system_info.power_options();
    QIcon item_icon(":/img/electrical.svg");

    ui.tree->addTopLevelItem(
        new Item(item_icon, tr("Power Source"), powerSourceToString(power_options.power_source())));
    ui.tree->addTopLevelItem(
        new Item(item_icon, tr("Battery Status"), batteryStatusToString(power_options.battery_status())));

    if (power_options.battery_status() != proto::system_info::PowerOptions::BATTERY_STATUS_NO_BATTERY &&
        power_options.battery_status() != proto::system_info::PowerOptions::BATTERY_STATUS_UNKNOWN)
    {
        ui.tree->addTopLevelItem(new Item(item_icon,
            tr("Battery Life Percent"),
            QString("%1%").arg(power_options.battery_life_percent())));

        if (power_options.full_battery_life_time() != 0)
        {
            ui.tree->addTopLevelItem(new Item(item_icon,
                tr("Full Battery Life Time"),
                delayToString(power_options.full_battery_life_time())));
        }

        if (power_options.remaining_battery_life_time() != 0)
        {
            ui.tree->addTopLevelItem(new Item(item_icon,
                tr("Remaining Battery Life Time"),
                delayToString(power_options.remaining_battery_life_time())));
        }
    }

    if (power_options.battery_size() == 0)
        ui.tree->setIndentation(0);
    else
        ui.tree->setIndentation(20);

    for (int i = 0; i < power_options.battery_size(); ++i)
    {
        const proto::system_info::PowerOptions::Battery& battery = power_options.battery(i);
        QList<QTreeWidgetItem*> group;

        if (!battery.device_name().empty())
            group << mk(tr("Device Name"), battery.device_name());

        if (!battery.manufacturer().empty())
            group << mk(tr("Manufacturer"), battery.manufacturer());

        if (!battery.manufacture_date().empty())
            group << mk(tr("Manufacture Date"), battery.manufacture_date());

        if (!battery.unique_id().empty())
            group << mk(tr("Unique Id"), battery.unique_id());

        if (!battery.serial_number().empty())
            group << mk(tr("Serial Number"), battery.serial_number());

        if (!battery.temperature().empty())
            group << mk(tr("Tempareture"), battery.temperature());

        if (battery.design_capacity() != 0)
            group << mk(tr("Design Capacity"), tr("%1 mWh").arg(battery.design_capacity()));

        if (!battery.type().empty())
            group << mk(tr("Type"), battery.type());

        if (battery.full_charged_capacity() != 0)
            group << mk(tr("Full Charged Capacity"), tr("%1 mWh").arg(battery.full_charged_capacity()));

        if (battery.depreciation() != 0)
            group << mk(tr("Depreciation"), QString("%1%").arg(battery.depreciation()));

        if (battery.current_capacity() != 0)
            group << mk(tr("Current Capacity"), tr("%1 mWh").arg(battery.current_capacity()));

        if (battery.voltage() != 0)
            group << mk(tr("Voltage"), tr("%1 mV").arg(battery.voltage()));

        QList<QTreeWidgetItem*> state_group;
        if (battery.state() != 0)
        {
            if (battery.state() & proto::system_info::PowerOptions::Battery::STATE_CHARGING)
                state_group << mk(tr("Charging"), tr("Yes"));

            if (battery.state() & proto::system_info::PowerOptions::Battery::STATE_CRITICAL)
                state_group << mk(tr("Critical"), tr("Yes"));

            if (battery.state() & proto::system_info::PowerOptions::Battery::STATE_DISCHARGING)
                state_group << mk(tr("Discharging"), tr("Yes"));

            if (battery.state() & proto::system_info::PowerOptions::Battery::STATE_POWER_ONLINE)
                state_group << mk(tr("Power OnLine"), tr("Yes"));
        }

        if (!state_group.isEmpty())
            group << new Item(tr("State"), state_group);

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/electrical.svg", tr("Battery #%1").arg(i + 1), group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetPowerOptions::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetPowerOptions::onContextMenu(const QPoint& point)
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

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetPowerOptions::powerSourceToString(
    proto::system_info::PowerOptions::PowerSource value)
{
    switch (value)
    {
        case proto::system_info::PowerOptions::POWER_SOURCE_DC_BATTERY:
            return tr("DC Battery");

        case proto::system_info::PowerOptions::POWER_SOURCE_AC_LINE:
            return tr("AC Line");

        default:
            return tr("Unknown");
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetPowerOptions::batteryStatusToString(
    proto::system_info::PowerOptions::BatteryStatus value)
{
    switch (value)
    {
        case proto::system_info::PowerOptions::BATTERY_STATUS_HIGH:
            return tr("High");

        case proto::system_info::PowerOptions::BATTERY_STATUS_LOW:
            return tr("Low");

        case proto::system_info::PowerOptions::BATTERY_STATUS_CRITICAL:
            return tr("Critical");

        case proto::system_info::PowerOptions::BATTERY_STATUS_CHARGING:
            return tr("Charging");

        case proto::system_info::PowerOptions::BATTERY_STATUS_NO_BATTERY:
            return tr("No Battery");

        default:
            return tr("Unknown");
    }
}

} // namespace client
