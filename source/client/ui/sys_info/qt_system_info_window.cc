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

#include "client/ui/sys_info/qt_system_info_window.h"

#include "base/logging.h"
#include "client/client_system_info.h"
#include "client/ui/sys_info/sys_info_widget_applications.h"
#include "client/ui/sys_info/sys_info_widget_connections.h"
#include "client/ui/sys_info/sys_info_widget_devices.h"
#include "client/ui/sys_info/sys_info_widget_drivers.h"
#include "client/ui/sys_info/sys_info_widget_env_vars.h"
#include "client/ui/sys_info/sys_info_widget_event_logs.h"
#include "client/ui/sys_info/sys_info_widget_licenses.h"
#include "client/ui/sys_info/sys_info_widget_monitors.h"
#include "client/ui/sys_info/sys_info_widget_net_adapters.h"
#include "client/ui/sys_info/sys_info_widget_net_shares.h"
#include "client/ui/sys_info/sys_info_widget_open_files.h"
#include "client/ui/sys_info/sys_info_widget_power_options.h"
#include "client/ui/sys_info/sys_info_widget_printers.h"
#include "client/ui/sys_info/sys_info_widget_processes.h"
#include "client/ui/sys_info/sys_info_widget_routes.h"
#include "client/ui/sys_info/sys_info_widget_services.h"
#include "client/ui/sys_info/sys_info_widget_summary.h"
#include "client/ui/sys_info/sys_info_widget_local_users.h"
#include "client/ui/sys_info/sys_info_widget_local_user_groups.h"
#include "client/ui/sys_info/sys_info_widget_video_adapters.h"
#include "client/ui/sys_info/tree_to_html.h"
#include "common/system_info_constants.h"
#include "qt_base/application.h"
#include "ui_qt_system_info_window.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>

Q_DECLARE_METATYPE(proto::system_info::SystemInfoRequest)
Q_DECLARE_METATYPE(proto::system_info::SystemInfo)

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

//--------------------------------------------------------------------------------------------------
QtSystemInfoWindow::QtSystemInfoWindow(std::shared_ptr<SessionState> session_state, QWidget* parent)
    : SessionWindow(session_state, parent),
      ui(std::make_unique<Ui::SystemInfoWindow>())
{
    LOG(LS_INFO) << "Ctor";

    ui->setupUi(this);

    QList<int> sizes;
    sizes.push_back(210);
    sizes.push_back(width() - 210);
    ui->splitter->setSizes(sizes);

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
    sys_info_widgets_.append(new SysInfoWidgetLicenses(this));
    sys_info_widgets_.append(new SysInfoWidgetApplications(this));
    sys_info_widgets_.append(new SysInfoWidgetOpenFiles(this));
    sys_info_widgets_.append(new SysInfoWidgetLocalUsers(this));
    sys_info_widgets_.append(new SysInfoWidgetLocalUserGroups(this));
    sys_info_widgets_.append(new SysInfoWidgetProcesses(this));

    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        if (i != 0)
            sys_info_widgets_[i]->hide();

        connect(sys_info_widgets_[i], &SysInfoWidget::sig_systemInfoRequest,
                this, &QtSystemInfoWindow::sig_systemInfoRequired);
    }

    CategoryItem* summary_category = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/computer.png",
        tr("Summary"),
        common::kSystemInfo_Summary);

    //----------------------------------------------------------------------------------------------
    // HARDWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* hardware_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/motherboard.png", tr("Hardware"));

    CategoryItem* devices = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/graphic-card.png",
        tr("Devices"),
        common::kSystemInfo_Devices);

    CategoryItem* video_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/graphic-card.png",
        tr("Video Adapters"),
        common::kSystemInfo_VideoAdapters);

    CategoryItem* monitors = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/monitor.png",
        tr("Monitors"),
        common::kSystemInfo_Monitors);

    CategoryItem* printers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/printer.png",
        tr("Printers"),
        common::kSystemInfo_Printers);

    CategoryItem* power_options = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/power-supply.png",
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
        CategoryItem::Type::ROOT_ITEM, ":/img/operating-system.png", tr("Software"));

    CategoryItem* applications = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/applications.png",
        tr("Applications"),
        common::kSystemInfo_Applications);

    CategoryItem* drivers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/graphic-card.png",
        tr("Drivers"),
        common::kSystemInfo_Drivers);

    CategoryItem* services = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/gear.png",
        tr("Services"),
        common::kSystemInfo_Services);

    CategoryItem* processes = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/application.png",
        tr("Processes"),
        common::kSystemInfo_Processes);

    CategoryItem* licenses = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/license-key.png",
        tr("Licenses"),
        common::kSystemInfo_Licenses);

    software_category->addChild(applications);
    software_category->addChild(drivers);
    software_category->addChild(services);
    software_category->addChild(processes);
    software_category->addChild(licenses);

    //----------------------------------------------------------------------------------------------
    // NETWORK
    //----------------------------------------------------------------------------------------------

    CategoryItem* network_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/graphic-card.png", tr("Network"));

    CategoryItem* network_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/graphic-card.png",
        tr("Network Adapters"),
        common::kSystemInfo_NetworkAdapters);

    CategoryItem* routes = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/servers-network.png",
        tr("Routes"),
        common::kSystemInfo_Routes);

    CategoryItem* connections = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/servers-network.png",
        tr("Connections"),
        common::kSystemInfo_Connections);

    CategoryItem* network_shares = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/folder-share.png",
        tr("Network Shares"),
        common::kSystemInfo_NetworkShares);

    CategoryItem* open_files = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/folder-share.png",
        tr("Open Files"),
        common::kSystemInfo_OpenFiles);

    network_category->addChild(network_adapters);
    network_category->addChild(routes);
    network_category->addChild(connections);
    network_category->addChild(network_shares);
    network_category->addChild(open_files);

    //----------------------------------------------------------------------------------------------
    // OPERATING SYSTEM
    //----------------------------------------------------------------------------------------------

    CategoryItem* os_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/operating-system.png", tr("Operating System"));

    CategoryItem* env_vars = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/block.png",
        tr("Environment Variables"),
        common::kSystemInfo_EnvironmentVariables);

    CategoryItem* event_logs = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/document-list.png",
        tr("Event Logs"),
        common::kSystemInfo_EventLogs);

    CategoryItem* local_users = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user.png",
        tr("Users"),
        common::kSystemInfo_LocalUsers);

    CategoryItem* local_user_groups = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/users.png",
        tr("User Groups"),
        common::kSystemInfo_LocalUserGroups);

    os_category->addChild(env_vars);
    os_category->addChild(event_logs);
    os_category->addChild(local_users);
    os_category->addChild(local_user_groups);

    //----------------------------------------------------------------------------------------------
    // TOP LEVEL CATEGORIES
    //----------------------------------------------------------------------------------------------

    ui->tree_category->addTopLevelItem(summary_category);
    ui->tree_category->addTopLevelItem(hardware_category);
    ui->tree_category->addTopLevelItem(software_category);
    ui->tree_category->addTopLevelItem(network_category);
    ui->tree_category->addTopLevelItem(os_category);

    for (int i = 0; i < ui->tree_category->topLevelItemCount(); ++i)
        ui->tree_category->expandItem(ui->tree_category->topLevelItem(i));

    ui->tree_category->setCurrentItem(summary_category);

    connect(ui->action_save, &QAction::triggered, this, [this]()
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

    connect(ui->action_print, &QAction::triggered, this, [this]()
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

    connect(ui->action_refresh, &QAction::triggered, this, &QtSystemInfoWindow::onRefresh);

    connect(ui->tree_category, &QTreeWidget::itemClicked,
            this, &QtSystemInfoWindow::onCategoryItemClicked);

    layout_ = new QHBoxLayout(ui->widget);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(sys_info_widgets_[current_widget_]);
}

//--------------------------------------------------------------------------------------------------
QtSystemInfoWindow::~QtSystemInfoWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Client> QtSystemInfoWindow::createClient()
{
    LOG(LS_INFO) << "Create client";

    std::unique_ptr<ClientSystemInfo> client = std::make_unique<ClientSystemInfo>(
        base::GuiApplication::ioTaskRunner());

    connect(this, &QtSystemInfoWindow::sig_systemInfoRequired,
            client.get(), &ClientSystemInfo::onSystemInfoRequest,
            Qt::QueuedConnection);
    connect(client.get(), &ClientSystemInfo::sig_start,
            this, &QtSystemInfoWindow::start,
            Qt::QueuedConnection);
    connect(client.get(), &ClientSystemInfo::sig_systemInfo,
            this, &QtSystemInfoWindow::setSystemInfo,
            Qt::QueuedConnection);

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void QtSystemInfoWindow::start()
{
    LOG(LS_INFO) << "Show window";

    for (int i = 0; i < sys_info_widgets_.size(); ++i)
    {
        if (sys_info_widgets_[i]->category() == common::kSystemInfo_Summary)
        {
            SysInfoWidgetSummary* summary = static_cast<SysInfoWidgetSummary*>(sys_info_widgets_[i]);

            summary->setClientVersion(base::Version::kCurrentFullVersion);
            summary->setHostVersion(sessionState()->hostVersion());
            summary->setRouterVersion(sessionState()->routerVersion());
            break;
        }
    }

    show();
    activateWindow();
    onRefresh();
}

//--------------------------------------------------------------------------------------------------
void QtSystemInfoWindow::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        SysInfoWidget* widget = sys_info_widgets_[i];

        if (widget->category() == system_info.footer().category())
            widget->setSystemInfo(system_info);
    }

    SysInfoWidget* current = sys_info_widgets_[current_widget_];
    if (current->treeWidget()->topLevelItemCount() == 0)
    {
        ui->action_save->setEnabled(false);
        ui->action_print->setEnabled(false);
    }
    else
    {
        ui->action_save->setEnabled(true);
        ui->action_print->setEnabled(true);
    }
}

//--------------------------------------------------------------------------------------------------
void QtSystemInfoWindow::onInternalReset()
{
    // TODO
}

//--------------------------------------------------------------------------------------------------
void QtSystemInfoWindow::onCategoryItemClicked(QTreeWidgetItem* item, int /* column */)
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

    SysInfoWidget* current = sys_info_widgets_[current_widget_];
    if (current->treeWidget()->topLevelItemCount() == 0)
    {
        ui->action_save->setEnabled(false);
        ui->action_print->setEnabled(false);
    }
    else
    {
        ui->action_save->setEnabled(true);
        ui->action_print->setEnabled(true);
    }
}

//--------------------------------------------------------------------------------------------------
void QtSystemInfoWindow::onRefresh()
{
    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        proto::system_info::SystemInfoRequest request = sys_info_widgets_[i]->request();
        emit sig_systemInfoRequired(request);
    }
}

} // namespace client
