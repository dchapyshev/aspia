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

#include "client/ui/sys_info/system_info_session_window.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>

#include "base/logging.h"
#include "base/version_constants.h"
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
#include "ui_system_info_session_window.h"

namespace client {

namespace {

class CategoryItem : public QTreeWidgetItem
{
public:
    enum class Type { ROOT_ITEM, CATEGORY_ITEM };

    CategoryItem(Type type, const QString& icon_path, const QString& text, const char* category = nullptr)
        : type_(type)
    {
        setIcon(0, QIcon(icon_path));
        setText(0, text);

        if (category)
            category_ = category;
    }

    Type type() const { return type_; }
    const std::string& category() const { return category_; }

private:
    Type type_;
    std::string category_;

    Q_DISABLE_COPY(CategoryItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
SystemInfoSessionWindow::SystemInfoSessionWindow(
    std::shared_ptr<SessionState> session_state, QWidget* parent)
    : SessionWindow(session_state, parent),
      ui(std::make_unique<Ui::SystemInfoSessionWindow>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    QList<int> sizes;
    sizes.emplace_back(220);
    sizes.emplace_back(width() - 220);
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
                this, &SystemInfoSessionWindow::sig_systemInfoRequired);
    }

    CategoryItem* summary_category = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/computer.svg",
        tr("Summary"),
        common::kSystemInfo_Summary);

    //----------------------------------------------------------------------------------------------
    // HARDWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* hardware_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Hardware"));

    CategoryItem* devices = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg",
        tr("Devices"),
        common::kSystemInfo_Devices);

    CategoryItem* video_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/video-card.svg",
        tr("Video Adapters"),
        common::kSystemInfo_VideoAdapters);

    CategoryItem* monitors = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/imac.svg",
        tr("Monitors"),
        common::kSystemInfo_Monitors);

    CategoryItem* printers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/printer.svg",
        tr("Printers"),
        common::kSystemInfo_Printers);

    CategoryItem* power_options = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/electrical.svg",
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
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Software"));

    CategoryItem* applications = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/software.svg",
        tr("Applications"),
        common::kSystemInfo_Applications);

    CategoryItem* drivers = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg",
        tr("Drivers"),
        common::kSystemInfo_Drivers);

    CategoryItem* services = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/gear.svg",
        tr("Services"),
        common::kSystemInfo_Services);

    CategoryItem* processes = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/heart-monitor.svg",
        tr("Processes"),
        common::kSystemInfo_Processes);

    CategoryItem* licenses = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/certificate.svg",
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
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Network"));

    CategoryItem* network_adapters = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg",
        tr("Network Adapters"),
        common::kSystemInfo_NetworkAdapters);

    CategoryItem* routes = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/flow-chart.svg",
        tr("Routes"),
        common::kSystemInfo_Routes);

    CategoryItem* connections = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/connected.svg",
        tr("Connections"),
        common::kSystemInfo_Connections);

    CategoryItem* network_shares = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg",
        tr("Network Shares"),
        common::kSystemInfo_NetworkShares);

    CategoryItem* open_files = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg",
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
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Operating System"));

    CategoryItem* env_vars = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/day-view.svg",
        tr("Environment Variables"),
        common::kSystemInfo_EnvironmentVariables);

    CategoryItem* event_logs = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/log.svg",
        tr("Event Logs"),
        common::kSystemInfo_EventLogs);

    CategoryItem* local_users = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user.svg",
        tr("Users"),
        common::kSystemInfo_LocalUsers);

    CategoryItem* local_user_groups = new CategoryItem(
        CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user-account.svg",
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

    connect(ui->action_refresh, &QAction::triggered, this, &SystemInfoSessionWindow::onRefresh);

    connect(ui->tree_category, &QTreeWidget::itemClicked,
            this, &SystemInfoSessionWindow::onCategoryItemClicked);

    layout_ = new QHBoxLayout(ui->widget);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(sys_info_widgets_[current_widget_]);
}

//--------------------------------------------------------------------------------------------------
SystemInfoSessionWindow::~SystemInfoSessionWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* SystemInfoSessionWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientSystemInfo* client = new ClientSystemInfo();

    connect(this, &SystemInfoSessionWindow::sig_systemInfoRequired,
            client, &ClientSystemInfo::onSystemInfoRequest,
            Qt::QueuedConnection);
    connect(client, &ClientSystemInfo::sig_showSessionWindow,
            this, &SystemInfoSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientSystemInfo::sig_systemInfo,
            this, &SystemInfoSessionWindow::onSystemInfoChanged,
            Qt::QueuedConnection);

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoSessionWindow::onShowWindow()
{
    LOG(INFO) << "Show window";

    for (int i = 0; i < sys_info_widgets_.size(); ++i)
    {
        if (sys_info_widgets_[i]->category() == common::kSystemInfo_Summary)
        {
            SysInfoWidgetSummary* summary = static_cast<SysInfoWidgetSummary*>(sys_info_widgets_[i]);

            summary->setClientVersion(base::kCurrentVersion);
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
void SystemInfoSessionWindow::onSystemInfoChanged(const proto::system_info::SystemInfo& system_info)
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
void SystemInfoSessionWindow::onInternalReset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SystemInfoSessionWindow::onCategoryItemClicked(QTreeWidgetItem* item, int /* column */)
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

            LOG(INFO) << "Current category changed:" << category << "(" << i << ")";

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
void SystemInfoSessionWindow::onRefresh()
{
    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        proto::system_info::SystemInfoRequest request = sys_info_widgets_[i]->request();
        emit sig_systemInfoRequired(request);
    }
}

} // namespace client
