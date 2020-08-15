//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/system_info_window.h"

#include "client/ui/tree_to_html.h"

#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>

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

    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(Item);
};

QTreeWidgetItem* mk(const QString& param, const QString& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);

    return item;
}

QTreeWidgetItem* mk(const QString& param, const std::string& value)
{
    return mk(param, QString::fromStdString(value));
}

void copyTextToClipboard(const QString& text)
{
    if (text.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(text);
}

} // namespace

SystemInfoWindow::SystemInfoWindow(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    connect(ui.action_save, &QAction::triggered, [this]()
    {
        QString file_path =
            QFileDialog::getSaveFileName(this,
                                         tr("HTML File"),
                                         QDir::homePath(),
                                         tr("HTML File (*.html)"));
        if (file_path.isEmpty())
            return;

        QString error_string;

        if (!treeToHtmlFile(ui.tree, file_path, &error_string))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to save file: %1").arg(error_string),
                                 QMessageBox::Ok);
        }
    });

    connect(ui.action_print, &QAction::triggered, [this]()
    {
        QString html = treeToHtmlString(ui.tree);

        QTextDocument document;
        document.setHtml(html);

        QPrinter printer;

        QPrintDialog dialog(&printer, this);
        if (dialog.exec() != QPrintDialog::Accepted)
            return;

        document.print(&printer);
    });

    connect(ui.action_refresh, &QAction::triggered, this, &SystemInfoWindow::systemInfoRequired);

    connect(ui.action_copy_row, &QAction::triggered, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_name, &QAction::triggered, [this]()
    {
        copyColumn(ui.tree->currentItem(), 0);
    });

    connect(ui.action_copy_value, &QAction::triggered, [this]()
    {
        copyColumn(ui.tree->currentItem(), 1);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SystemInfoWindow::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

SystemInfoWindow::~SystemInfoWindow() = default;

void SystemInfoWindow::setSystemInfo(const proto::SystemInfo& system_info)
{
    ui.tree->clear();

    if (system_info.has_computer())
    {
        const proto::system_info::Computer& computer = system_info.computer();
        QList<QTreeWidgetItem*> items;

        if (!computer.name().empty())
            items << mk(tr("Name"), computer.name());

        if (!computer.domain().empty())
            items << mk(tr("Domain"), computer.domain());

        if (!computer.workgroup().empty())
            items << mk(tr("Workgroup"), computer.workgroup());

        if (computer.uptime())
            items << mk(tr("Uptime"), delayToString(computer.uptime()));

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/computer.png", tr("Computer"), items));
        }
    }

    if (system_info.has_operating_system())
    {
        const proto::system_info::OperatingSystem& os = system_info.operating_system();
        QList<QTreeWidgetItem*> items;

        if (!os.name().empty())
            items << mk(tr("Name"), os.name());

        if (!os.version().empty())
            items << mk(tr("Version"), os.version());

        if (!os.arch().empty())
            items << mk(tr("Architecture"), os.arch());

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/operating-system.png", tr("Operating System"), items));
        }
    }

    if (system_info.has_motherboard())
    {
        const proto::system_info::Motherboard& motherboard = system_info.motherboard();
        QList<QTreeWidgetItem*> items;

        if (!motherboard.manufacturer().empty())
            items << mk(tr("Manufacturer"), motherboard.manufacturer());

        if (!motherboard.model().empty())
            items << mk(tr("Model"), motherboard.model());

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/motherboard.png", tr("Motherboard"), items));
        }
    }

    if (system_info.has_processor())
    {
        const proto::system_info::Processor& processor = system_info.processor();
        QList<QTreeWidgetItem*> items;

        if (!processor.model().empty())
            items << mk(tr("Model"), processor.model());

        if (!processor.vendor().empty())
            items << mk(tr("Vendor"), processor.vendor());

        if (processor.packages())
            items << mk(tr("Packages"), QString::number(processor.packages()));

        if (processor.cores())
            items << mk(tr("Cores"), QString::number(processor.cores()));

        if (processor.threads())
            items << mk(tr("Threads"), QString::number(processor.threads()));

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/processor.png", tr("Processor"), items));
        }
    }

    if (system_info.has_bios())
    {
        const proto::system_info::Bios& bios = system_info.bios();
        QList<QTreeWidgetItem*> items;

        if (!bios.vendor().empty())
            items << mk(tr("Vendor"), bios.vendor());

        if (!bios.version().empty())
            items << mk(tr("Version"), bios.version());

        if (!bios.date().empty())
            items << mk(tr("Date"), bios.date());

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/bios.png", "BIOS", items));
        }
    }

    if (system_info.has_memory())
    {
        const proto::system_info::Memory& memory = system_info.memory();
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < memory.module_size(); ++i)
        {
            const proto::system_info::Memory::Module& module = memory.module(i);
            QList<QTreeWidgetItem*> group;

            if (module.present())
            {
                if (!module.manufacturer().empty())
                    group << mk(tr("Manufacturer"), module.manufacturer());

                if (module.size())
                    group << mk(tr("Size"), sizeToString(module.size()));

                if (module.speed())
                    group << mk(tr("Speed"), tr("%1 MHz").arg(module.speed()));

                if (!module.type().empty())
                    group << mk(tr("Type"), module.type());

                if (!module.form_factor().empty())
                    group << mk(tr("Form Factor"), module.form_factor());

                if (!module.part_number().empty())
                    group << mk(tr("Part Number"), module.part_number());
            }
            else
            {
                group << mk(tr("Installed"), tr("No"));
            }

            if (!group.isEmpty())
                items << new Item(QString::fromStdString(module.location()), group);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/memory.png", tr("Memory"), items));
        }
    }

    if (system_info.has_logical_drives())
    {
        const proto::system_info::LogicalDrives& drives = system_info.logical_drives();
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < drives.drive_size(); ++i)
        {
            const proto::system_info::LogicalDrives::Drive& drive = drives.drive(i);

            QString param;
            QString value;

            if (drive.file_system().empty())
            {
                param = QString::fromStdString(drive.path());
            }
            else
            {
                param = QString("%1 (%2)")
                    .arg(QString::fromStdString(drive.path()))
                    .arg(QString::fromStdString(drive.file_system()));
            }

            if (drive.total_size() && drive.total_size() != -1)
            {
                value = tr("%1 (%2 free)")
                    .arg(sizeToString(drive.total_size()))
                    .arg(sizeToString(drive.free_size()));
            }

            items << mk(param, value);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/drive.png", tr("Logical Drives"), items));
        }
    }

    if (system_info.has_network_adapters())
    {
        const proto::system_info::NetworkAdapters& adapters = system_info.network_adapters();
        QList<QTreeWidgetItem*> items;

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
                    .arg(QString::fromStdString(adapter.address(j).ip()))
                    .arg(QString::fromStdString(adapter.address(j).mask()));

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
                items << new Item(QString::fromStdString(adapter.connection_name()), group);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/graphic-card.png", tr("Network Connections"), items));
        }
    }

    if (system_info.has_printers())
    {
        const proto::system_info::Printers& printers = system_info.printers();
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < printers.printer_size(); ++i)
        {
            const proto::system_info::Printers::Printer& printer = printers.printer(i);
            QList<QTreeWidgetItem*> group;

            group << mk(tr("Default"), printer.default_() ? tr("Yes") : tr("No"));

            if (!printer.port().empty())
                group << mk(tr("Port"), printer.port());

            if (!printer.driver().empty())
                group << mk(tr("Driver"), printer.driver());

            group << mk(tr("Shared"), printer.shared() ? tr("Yes") : tr("No"));

            if (printer.shared() && !printer.share_name().empty())
                group << mk(tr("Share Name"), printer.share_name());

            group << mk(tr("Jobs Count"), QString::number(printer.jobs_count()));

            if (!group.isEmpty())
                items << new Item(QString::fromStdString(printer.name()), group);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/printer.png", tr("Printers"), items));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

void SystemInfoWindow::onContextMenu(const QPoint& point)
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

void SystemInfoWindow::copyRow(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QString name = item->text(0);
    QString value = item->text(1);

    if (value.isEmpty())
        copyTextToClipboard(name);
    else
        copyTextToClipboard(name + QLatin1String(": ") + value);
}

void SystemInfoWindow::copyColumn(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    copyTextToClipboard(item->text(column));
}

// static
QString SystemInfoWindow::sizeToString(int64_t size)
{
    static const int64_t kKB = 1024LL;
    static const int64_t kMB = kKB * 1024LL;
    static const int64_t kGB = kMB * 1024LL;
    static const int64_t kTB = kGB * 1024LL;

    QString units;
    int64_t divider;

    if (size >= kTB)
    {
        units = tr("TB");
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = tr("GB");
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = tr("MB");
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = tr("kB");
        divider = kKB;
    }
    else
    {
        units = tr("B");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

QString SystemInfoWindow::delayToString(uint64_t delay)
{
    uint64_t days = (delay / 86400);
    uint64_t hours = (delay % 86400) / 3600;
    uint64_t minutes = ((delay % 86400) % 3600) / 60;
    uint64_t seconds = ((delay % 86400) % 3600) % 60;

    QString seconds_string = tr("%n seconds", "", seconds);
    QString minutes_string = tr("%n minutes", "", minutes);
    QString hours_string = tr("%n hours", "", hours);

    if (!days)
    {
        if (!hours)
        {
            if (!minutes)
            {
                return seconds_string;
            }
            else
            {
                return minutes_string + QLatin1Char(' ') + seconds_string;
            }
        }
        else
        {
            return hours_string + QLatin1Char(' ') +
                   minutes_string + QLatin1Char(' ') +
                   seconds_string;
        }
    }
    else
    {
        QString days_string = tr("%n days", "", days);

        return days_string + QLatin1Char(' ') +
               hours_string + QLatin1Char(' ') +
               minutes_string + QLatin1Char(' ') +
               seconds_string;
    }
}

// static
QString SystemInfoWindow::speedToString(uint64_t speed)
{
    static const uint64_t kKbps = 1000ULL;
    static const uint64_t kMbps = kKbps * 1000ULL;
    static const uint64_t kGbps = kMbps * 1000ULL;

    QString units;
    uint64_t divider;

    if (speed >= kGbps)
    {
        units = tr("Gbps");
        divider = kGbps;
    }
    else if (speed >= kMbps)
    {
        units = tr("Mbps");
        divider = kMbps;
    }
    else if (speed >= kKbps)
    {
        units = tr("Kbps");
        divider = kKbps;
    }
    else
    {
        units = tr("bps");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(speed) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

} // namespace client
