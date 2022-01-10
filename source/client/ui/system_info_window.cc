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

#include "base/logging.h"
#include "client/ui/sys_info_widget_connections.h"
#include "client/ui/sys_info_widget_devices.h"
#include "client/ui/sys_info_widget_drivers.h"
#include "client/ui/sys_info_widget_env_vars.h"
#include "client/ui/sys_info_widget_event_logs.h"
#include "client/ui/sys_info_widget_monitors.h"
#include "client/ui/sys_info_widget_net_adapters.h"
#include "client/ui/sys_info_widget_net_shares.h"
#include "client/ui/sys_info_widget_power_options.h"
#include "client/ui/sys_info_widget_printers.h"
#include "client/ui/sys_info_widget_ras.h"
#include "client/ui/sys_info_widget_routes.h"
#include "client/ui/sys_info_widget_services.h"
#include "client/ui/sys_info_widget_summary.h"
#include "client/ui/sys_info_widget_video_adapters.h"
#include "client/ui/tree_to_html.h"
#include "common/desktop_session_constants.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>

namespace client {

namespace {

class CategoryItem : public QTreeWidgetItem
{
public:
    enum class Type { ROOT_ITEM, CATEGORY_ITEM };

    CategoryItem(Type type, const QString& icon_path, const QString& text, const char* category = nullptr)
        : type_(type)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        if (category)
            category_ = category;
    }

    Type type() const { return type_; }
    const std::string& category() const { return category_; }

private:
    Type type_;
    std::string category_;

    DISALLOW_COPY_AND_ASSIGN(CategoryItem);
};

} // namespace

SystemInfoWindow::SystemInfoWindow(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    QList<int> sizes;
    sizes.push_back(210);
    sizes.push_back(width() - 210);
    ui.splitter->setSizes(sizes);

    sys_info_widgets_.append(new SysInfoWidgetSummary(this));
    sys_info_widgets_.append(new SysInfoWidgetDevices(this));
    sys_info_widgets_.append(new SysInfoWidgetVideoAdapters(this));
    sys_info_widgets_.append(new SysInfoWidgetMonitors(this));
    sys_info_widgets_.append(new SysInfoWidgetNetAdapters(this));
    sys_info_widgets_.append(new SysInfoWidgetNetShares(this));
    sys_info_widgets_.append(new SysInfoWidgetPowerOptions(this));
    sys_info_widgets_.append(new SysInfoWidgetPrinters(this));
    sys_info_widgets_.append(new SysInfoWidgetDrivers(this));
    sys_info_widgets_.append(new SysInfoWidgetServices(this));
    sys_info_widgets_.append(new SysInfoWidgetEnvVars(this));
    sys_info_widgets_.append(new SysInfoWidgetEventLogs(this));
    sys_info_widgets_.append(new SysInfoWidgetRoutes(this));
    sys_info_widgets_.append(new SysInfoWidgetConnections(this));
    sys_info_widgets_.append(new SysInfoWidgetRas(this));

    for (int i = 1; i < sys_info_widgets_.count(); ++i)
        sys_info_widgets_[i]->hide();

    CategoryItem* summary_category = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM, QStringLiteral(":/img/computer.png"), tr("Summary"));

    //----------------------------------------------------------------------------------------------
    // HARDWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* hardware_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, QStringLiteral(":/img/motherboard.png"), tr("Hardware"));

    CategoryItem* devices = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/graphic-card.png"),
        tr("Devices"),
        common::kSystemInfo_Devices);

    CategoryItem* video_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/graphic-card.png"),
        tr("Video Adapters"),
        common::kSystemInfo_VideoAdapters);

    CategoryItem* monitors = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/monitor.png"),
        tr("Monitors"),
        common::kSystemInfo_Monitors);

    CategoryItem* printers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/printer.png"),
        tr("Printers"),
        common::kSystemInfo_Printers);

    CategoryItem* power_options = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/power-supply.png"),
        tr("Power Options"),
        common::kSystemInfo_PowerOptions);

    hardware_category->addChild(devices);
    hardware_category->addChild(video_adapters);
    hardware_category->addChild(monitors);
    hardware_category->addChild(printers);
    hardware_category->addChild(power_options);

    //----------------------------------------------------------------------------------------------
    // SOFTWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* software_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, QStringLiteral(":/img/operating-system.png"), tr("Software"));

    CategoryItem* drivers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/graphic-card.png"),
        tr("Drivers"),
        common::kSystemInfo_Drivers);

    CategoryItem* services = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/gear.png"),
        tr("Services"),
        common::kSystemInfo_Services);

    CategoryItem* env_vars = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/block.png"),
        tr("Environment Variables"),
        common::kSystemInfo_EnvironmentVariables);

    CategoryItem* event_logs = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/document-list.png"),
        tr("Event Logs"),
        common::kSystemInfo_EventLogs);

    software_category->addChild(drivers);
    software_category->addChild(services);
    software_category->addChild(env_vars);
    software_category->addChild(event_logs);

    //----------------------------------------------------------------------------------------------
    // NETWORK
    //----------------------------------------------------------------------------------------------

    CategoryItem* network_category = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM, QStringLiteral(":/img/graphic-card.png"), tr("Network"));

    CategoryItem* network_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/graphic-card.png"),
        tr("Network Adapters"),
        common::kSystemInfo_NetworkAdapters);

    CategoryItem* routes = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/servers-network.png"),
        tr("Routes"),
        common::kSystemInfo_Routes);

    CategoryItem* connections = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/servers-network.png"),
        tr("Connections"),
        common::kSystemInfo_Connections);

    CategoryItem* network_shares = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/folder-share.png"),
        tr("Network Shares"),
        common::kSystemInfo_NetworkShares);

    CategoryItem* ras = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        QStringLiteral(":/img/globe-network-ethernet.png"),
        tr("RAS"),
        common::kSystemInfo_Ras);

    network_category->addChild(network_adapters);
    network_category->addChild(routes);
    network_category->addChild(connections);
    network_category->addChild(network_shares);
    network_category->addChild(ras);

    //----------------------------------------------------------------------------------------------
    // TOP LEVEL CATEGORIES
    //----------------------------------------------------------------------------------------------

    ui.tree_category->addTopLevelItem(summary_category);
    ui.tree_category->addTopLevelItem(hardware_category);
    ui.tree_category->addTopLevelItem(software_category);
    ui.tree_category->addTopLevelItem(network_category);

    for (int i = 0; i < ui.tree_category->topLevelItemCount(); ++i)
        ui.tree_category->expandItem(ui.tree_category->topLevelItem(i));

    ui.tree_category->setCurrentItem(summary_category);

    connect(ui.action_save, &QAction::triggered, this, [this]()
    {
        QString file_path =
            QFileDialog::getSaveFileName(this,
                                         tr("HTML File"),
                                         QDir::homePath(),
                                         tr("HTML File (*.html)"));
        if (file_path.isEmpty())
            return;

        QString error_string;

        if (!treeToHtmlFile(sys_info_widgets_[current_widget_]->treeWidget(),
                            file_path,
                            &error_string))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to save file: %1").arg(error_string),
                                 QMessageBox::Ok);
        }
    });

    connect(ui.action_print, &QAction::triggered, this, [this]()
    {
        QString html = treeToHtmlString(sys_info_widgets_[current_widget_]->treeWidget());

        QTextDocument document;
        document.setHtml(html);

        QPrinter printer;

        QPrintDialog dialog(&printer, this);
        if (dialog.exec() != QPrintDialog::Accepted)
            return;

        document.print(&printer);
    });

    connect(ui.action_refresh, &QAction::triggered, this, [this]()
    {
        ui.tree_category->setEnabled(false);
        ui.widget->setEnabled(false);

        sendSystemInfoRequest(sys_info_widgets_[current_widget_]->category());
    });

    connect(ui.tree_category, &QTreeWidget::itemClicked,
            this, &SystemInfoWindow::onCategoryItemClicked);

    layout_ = new QHBoxLayout(ui.widget);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(sys_info_widgets_[current_widget_]);
}

SystemInfoWindow::~SystemInfoWindow() = default;

void SystemInfoWindow::setSystemInfo(const proto::SystemInfo& system_info)
{
    ui.tree_category->setEnabled(true);
    ui.widget->setEnabled(true);

    if (system_info.footer().category().empty())
    {
        for (int i = 0; i < sys_info_widgets_.count(); ++i)
            sys_info_widgets_[i]->setSystemInfo(system_info);
    }
    else
    {
        sys_info_widgets_[current_widget_]->setSystemInfo(system_info);
    }
}

void SystemInfoWindow::onCategoryItemClicked(QTreeWidgetItem* item, int /* column */)
{
    CategoryItem* category_item = static_cast<CategoryItem*>(item);
    if (!category_item)
        return;

    if (category_item->type() == CategoryItem::Type::ROOT_ITEM)
        return;

    const std::string& category = category_item->category();

    layout_->removeWidget(sys_info_widgets_[current_widget_]);
    sys_info_widgets_[current_widget_]->setVisible(false);

    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        SysInfoWidget* widget = sys_info_widgets_[i];

        if (widget->category() == category)
        {
            current_widget_ = i;

            LOG(LS_INFO) << "Current category changed: " << category << " (" << i << ")";

            layout_->addWidget(widget);
            widget->setVisible(true);
            break;
        }
    }
}

void SystemInfoWindow::sendSystemInfoRequest(const std::string& category)
{
    proto::SystemInfoRequest request;
    request.set_category(category);

    emit systemInfoRequired(request.SerializeAsString());
}

} // namespace client
