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

#include "common/sys_info/sys_info_widget_cpu.h"

#include <QMenu>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_cpu.h"

namespace {

const char kCpuIcon[] = ":/img/microchip.svg";
const char kCacheIcon[] = ":/img/processor.svg";
const char kInstructionSetIcon[] = ":/img/integrated-circuit.svg";
const char kSecurityIcon[] = ":/img/lock.svg";
const char kPowerIcon[] = ":/img/electrical.svg";
const char kVirtualizationIcon[] = ":/img/virtual-machine.svg";
const char kFeatureIcon[] = ":/img/feature.svg";
const char kSupportedIcon[] = ":/img/check.svg";
const char kUnsupportedIcon[] = ":/img/cancel.svg";

// The groups of features, in the order they are shown. The host fills a list of its own for each
// of them, so the page only decides what to call them.
enum FeatureGroup
{
    GROUP_INSTRUCTION_SET = 0,
    GROUP_SECURITY,
    GROUP_POWER,
    GROUP_VIRTUALIZATION,
    GROUP_OTHER,
    GROUP_COUNT
};

class Item : public QTreeWidgetItem
{
public:
    Item(const QString& icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        // A child that came with an icon of its own keeps it.
        for (const auto& child : childs)
        {
            if (child->icon(0).isNull())
                child->setIcon(0, icon);

            for (int i = 0; i < child->childCount(); ++i)
            {
                if (child->child(i)->icon(0).isNull())
                    child->child(i)->setIcon(0, icon);
            }
        }

        addChildren(childs);
    }

    // Group of parameters inside a group.
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
SysInfoWidgetCpu::SysInfoWidgetCpu(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoCpu>())
{
    ui->setupUi(this);

    connect(ui->action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui->tree->currentItem());
    });

    connect(ui->action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 0);
    });

    connect(ui->action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 1);
    });

    connect(ui->tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetCpu::onContextMenu);

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetCpu::~SysInfoWidgetCpu() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetCpu::category() const
{
    return kSystemInfo_Processor;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetCpu::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui->tree->clear();

    // A report arrives without the processor when it was not asked about it. The next report may
    // still bring it.
    const bool has_processor = system_info.has_processor();

    ui->tree->setEnabled(has_processor);

    if (!has_processor)
        return;

    const proto::system_info::Processor& cpu = system_info.processor();

    QList<QTreeWidgetItem*> groups;

    const QList<QTreeWidgetItem*> properties = identityParameters(cpu);
    if (!properties.isEmpty())
        groups << new Item(kCpuIcon, tr("Processor Properties"), properties);

    const QList<QTreeWidgetItem*> caches = cacheParameters(cpu);
    if (!caches.isEmpty())
        groups << new Item(kCacheIcon, tr("Caches"), caches);

    const QString group_names[GROUP_COUNT] =
    {
        tr("Instruction Set"),
        tr("Security Features"),
        tr("Power Management Features"),
        tr("Virtualization Features"),
        tr("Other Features")
    };

    const char* group_icons[GROUP_COUNT] =
    {
        kInstructionSetIcon,
        kSecurityIcon,
        kPowerIcon,
        kVirtualizationIcon,
        kFeatureIcon
    };

    // A group the host had nothing to say about is not shown at all.
    for (int group = 0; group < GROUP_COUNT; ++group)
    {
        const QList<QTreeWidgetItem*> features = featureParameters(cpu, group);
        if (features.isEmpty())
            continue;

        groups << new Item(group_icons[group], group_names[group], features);
    }

    ui->tree->addTopLevelItems(groups);

    // The identification of the processor is what the category is opened for. The lists of the
    // features are long and stay collapsed until they are asked for.
    for (int i = 0; i < ui->tree->topLevelItemCount() && i < 2; ++i)
        ui->tree->expandItem(ui->tree->topLevelItem(i));

    if (!isStateRestored())
    {
        for (int i = 0; i < ui->tree->columnCount(); ++i)
            ui->tree->resizeColumnToContents(i);
    }
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetCpu::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetCpu::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui->tree->itemAt(point);
    if (!current_item)
        return;

    ui->tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui->action_copy_row);
    menu.addAction(ui->action_copy_name);
    menu.addAction(ui->action_copy_value);

    menu.exec(ui->tree->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::identityParameters(
    const proto::system_info::Processor& cpu) const
{
    QList<QTreeWidgetItem*> items;

    // What the processor calls itself depends on its architecture, so the values arrive named.
    for (int i = 0; i < cpu.identity_size(); ++i)
    {
        const proto::system_info::Processor::Identity& identity = cpu.identity(i);

        items << mk(QString::fromStdString(identity.name()),
                    QString::fromStdString(identity.value()));
    }

    if (cpu.packages())
        items << mk(tr("Packages"), QString::number(cpu.packages()));

    if (cpu.cores())
        items << mk(tr("Physical Cores"), QString::number(cpu.cores()));

    if (cpu.threads())
        items << mk(tr("Logical Cores"), QString::number(cpu.threads()));

    if (cpu.temperature())
    {
        items << mk(tr("Temperature"),
                    tr("%1 C").arg(QString::number(cpu.temperature() / 10.0, 'f', 1)));
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::cacheParameters(
    const proto::system_info::Processor& cpu) const
{
    QList<QTreeWidgetItem*> items;

    for (int i = 0; i < cpu.cache_size(); ++i)
    {
        const proto::system_info::Processor::Cache& cache = cpu.cache(i);

        QString title;

        switch (cache.type())
        {
            case proto::system_info::Processor::Cache::TYPE_DATA:
                title = tr("L%1 Data Cache").arg(cache.level());
                break;

            case proto::system_info::Processor::Cache::TYPE_INSTRUCTION:
                title = tr("L%1 Instruction Cache").arg(cache.level());
                break;

            default:
                title = tr("L%1 Cache").arg(cache.level());
                break;
        }

        QList<QTreeWidgetItem*> params;

        if (cache.size())
            params << mk(tr("Size"), Formatter::sizeToString(cache.size()));

        // A fully associative cache has no number of ways to name.
        if (cache.fully_associative())
            params << mk(tr("Associativity"), tr("Fully associative"));
        else if (cache.ways())
            params << mk(tr("Associativity"), tr("%1-way").arg(cache.ways()));

        if (cache.line_size())
            params << mk(tr("Line Size"), tr("%1 bytes").arg(cache.line_size()));

        if (cache.sets())
            params << mk(tr("Sets"), QString::number(cache.sets()));

        if (cache.threads())
            params << mk(tr("Shared By"), tr("%1 threads").arg(cache.threads()));

        items << new Item(title, params);
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::featureParameters(
    const proto::system_info::Processor& cpu, int group) const
{
    const QIcon supported_icon(kSupportedIcon);
    const QIcon unsupported_icon(kUnsupportedIcon);

    QList<QTreeWidgetItem*> items;

    const auto add = [&](const auto& features)
    {
        for (const auto& feature : features)
        {
            const bool is_supported = feature.supported();

            QTreeWidgetItem* item = mk(QString::fromStdString(feature.name()),
                                       is_supported ? tr("Yes") : tr("No"));
            item->setIcon(0, is_supported ? supported_icon : unsupported_icon);

            items << item;
        }
    };

    switch (group)
    {
        case GROUP_INSTRUCTION_SET: add(cpu.instruction_set()); break;
        case GROUP_SECURITY:        add(cpu.security()); break;
        case GROUP_POWER:           add(cpu.power_management()); break;
        case GROUP_VIRTUALIZATION:  add(cpu.virtualization()); break;
        default:                    add(cpu.other()); break;
    }

    return items;
}
