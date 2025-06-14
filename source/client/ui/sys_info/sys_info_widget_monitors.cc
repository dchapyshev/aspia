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

#include "client/ui/sys_info/sys_info_widget_monitors.h"

#include <QMenu>

#include <cmath>
#include <limits>

#include "common/system_info_constants.h"

namespace client {

namespace {

//--------------------------------------------------------------------------------------------------
bool isDoubleEqual(double first, double second)
{
    return (std::fabs(first - second) < std::numeric_limits<double>::epsilon());
}

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

    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
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
SysInfoWidgetMonitors::SysInfoWidgetMonitors(QWidget* parent)
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
            this, &SysInfoWidgetMonitors::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetMonitors::~SysInfoWidgetMonitors() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetMonitors::category() const
{
    return common::kSystemInfo_Monitors;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetMonitors::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_monitors())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Monitors& monitors = system_info.monitors();

    for (int i = 0; i < monitors.monitor_size(); ++i)
    {
        const proto::system_info::Monitors::Monitor& monitor = monitors.monitor(i);
        QList<QTreeWidgetItem*> group;

        if (monitor.system_name().empty())
            continue;

        if (!monitor.monitor_name().empty())
            group << mk(tr("Monitor Name"), monitor.monitor_name());

        if (!monitor.manufacturer_name().empty())
            group << mk(tr("Manufacturer Name"), monitor.manufacturer_name());

        if (!monitor.monitor_id().empty())
            group << mk(tr("Monitor ID"), monitor.monitor_id());

        if (!monitor.serial_number().empty())
            group << mk(tr("Serial Number"), monitor.serial_number());

        if (monitor.edid_version() != 0)
        {
            group << mk(tr("EDID Version"),
                        QString("%1.%2").arg(monitor.edid_version()).arg(monitor.edid_revision()));
        }

        if (monitor.week_of_manufacture() != 0 && monitor.year_of_manufacture() != 0)
        {
            group << mk(tr("Date Of Manufacture"),
                        tr("Week %1 / %2").arg(monitor.week_of_manufacture())
                                          .arg(monitor.year_of_manufacture()));
        }

        if (!isDoubleEqual(monitor.gamma(), 0.0))
            group << mk(tr("Gamma"), QString::number(monitor.gamma(), 'f', 1));

        if (monitor.max_horizontal_image_size() != 0 && monitor.max_vertical_image_size() != 0)
        {
            group << mk(tr("Image Size"),
                        tr("%1x%2 cm").arg(monitor.max_horizontal_image_size())
                                      .arg(monitor.max_vertical_image_size()));

            // Calculate the monitor diagonal by the Pythagorean theorem and translate from
            // centimeters to inches.
            double diagonal_size =
                sqrt((monitor.max_horizontal_image_size() * monitor.max_horizontal_image_size()) +
                (monitor.max_vertical_image_size() * monitor.max_vertical_image_size())) / 2.54;

            group << mk(tr("Diagonal Size"), QString("%1\"").arg(diagonal_size, 2, 'f', 1));
        }

        if (monitor.horizontal_resolution() != 0 && monitor.vertical_resoulution() != 0)
        {
            group << mk(tr("Resolution"),
                        QString("%1x%2").arg(monitor.horizontal_resolution())
                                        .arg(monitor.vertical_resoulution()));
        }

        if (monitor.min_horizontal_rate() != 0 && monitor.max_horizontal_rate() != 0)
        {
            group << mk(tr("Horizontal Frequency"),
                        tr("%1 - %2 kHz").arg(monitor.min_horizontal_rate())
                                         .arg(monitor.max_horizontal_rate()));
        }

        if (monitor.min_vertical_rate() != 0 && monitor.max_vertical_rate() != 0)
        {
            group << mk(tr("Vertical Frequency"),
                        tr("%1 - %2 Hz").arg(monitor.min_vertical_rate())
                                        .arg(monitor.max_vertical_rate()));
        }

        if (!isDoubleEqual(monitor.pixel_clock(), 0.0))
            group << mk(tr("Pixel Clock"), tr("%1 MHz").arg(monitor.pixel_clock()));

        if (monitor.max_pixel_clock() != 0)
            group << mk(tr("Maximum Pixel Clock"), tr("%1 MHz").arg(monitor.max_pixel_clock()));

        group << mk(tr("Input Signal Type"), inputSignalTypeToString(monitor.input_signal_type()));

        QList<QTreeWidgetItem*> features_group;
        features_group << mk("Default GTF", monitor.default_gtf_supported() ? tr("Yes") : tr("No"));
        features_group << mk("Suspend", monitor.suspend_supported() ? tr("Yes") : tr("No"));
        features_group << mk("Standby", monitor.standby_supported() ? tr("Yes") : tr("No"));
        features_group << mk("Active Off", monitor.active_off_supported() ? tr("Yes") : tr("No"));
        features_group << mk("Preferred Timing Mode", monitor.preferred_timing_mode_supported() ? tr("Yes") : tr("No"));
        features_group << mk("sRGB", monitor.srgb_supported() ? tr("Yes") : tr("No"));

        if (!features_group.isEmpty())
            group << new Item(tr("Supported Features"), features_group);

        QList<QTreeWidgetItem*> timings_group;

        for (int j = 0; j < monitor.timings_size(); ++j)
        {
            const proto::system_info::Monitors::Monitor::Timing& timing = monitor.timings(j);

            timings_group << mk(QString("%1x%2").arg(timing.width()).arg(timing.height()),
                                tr("%1 Hz").arg(timing.frequency()));
        }

        if (!timings_group.isEmpty())
            group << new Item(tr("Supported Video Modes"), timings_group);

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/monitor.png", QString::fromStdString(monitor.system_name()), group));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetMonitors::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetMonitors::onContextMenu(const QPoint& point)
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
QString SysInfoWidgetMonitors::inputSignalTypeToString(
    proto::system_info::Monitors::Monitor::InputSignalType value)
{
    switch (value)
    {
        case proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_DIGITAL:
            return tr("Digital");

        case proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_ANALOG:
            return tr("Analog");

        default:
            return tr("Unknown");
    }
}

} // namespace client
