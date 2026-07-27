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

#include "common/sys_info/sys_info_view.h"

#include <QDataStream>
#include <QDir>
#include <QFileDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QIODevice>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVersionNumber>

#include "base/logging.h"
#include "common/system_info_constants.h"
#include "common/desktop/msg_box.h"
#include "common/sys_info/sys_info_widget_applications.h"
#include "common/sys_info/sys_info_widget_connections.h"
#include "common/sys_info/sys_info_widget_devices.h"
#include "common/sys_info/sys_info_widget_drivers.h"
#include "common/sys_info/sys_info_widget_drives.h"
#include "common/sys_info/sys_info_widget_env_vars.h"
#include "common/sys_info/sys_info_widget_event_logs.h"
#include "common/sys_info/sys_info_widget_licenses.h"
#include "common/sys_info/sys_info_widget_local_user_groups.h"
#include "common/sys_info/sys_info_widget_local_users.h"
#include "common/sys_info/sys_info_widget_monitors.h"
#include "common/sys_info/sys_info_widget_net_adapters.h"
#include "common/sys_info/sys_info_widget_net_shares.h"
#include "common/sys_info/sys_info_widget_open_files.h"
#include "common/sys_info/sys_info_widget_power_options.h"
#include "common/sys_info/sys_info_widget_printers.h"
#include "common/sys_info/sys_info_widget_processes.h"
#include "common/sys_info/sys_info_widget_routes.h"
#include "common/sys_info/sys_info_widget_services.h"
#include "common/sys_info/sys_info_widget_summary.h"
#include "common/sys_info/sys_info_widget_video_adapters.h"
#include "common/sys_info/tree_to_html.h"
#include "proto/system_info.h"
#include "ui_sys_info_view.h"

namespace {

// Width the category tree starts with.
constexpr int kCategoryTreeWidth = 220;

// Size the view asks its window for when nothing was restored.
constexpr int kPreferredWidth = 990;
constexpr int kPreferredHeight = 703;

class CategoryItem final : public QTreeWidgetItem
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

    Q_DISABLE_COPY_MOVE(CategoryItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoView::SysInfoView(QWidget* parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::SysInfoView>())
{
    ui->setupUi(this);

    QList<int> sizes;
    sizes.emplace_back(kCategoryTreeWidth);
    sizes.emplace_back(kPreferredWidth - kCategoryTreeWidth);
    ui->splitter->setSizes(sizes);

    summary_widget_ = new SysInfoWidgetSummary(this);

    sys_info_widgets_.append(summary_widget_);
    sys_info_widgets_.append(new SysInfoWidgetDevices(this));
    sys_info_widgets_.append(new SysInfoWidgetDrives(this));
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
                this, &SysInfoView::sig_systemInfoRequest);
    }

    buildCategoryTree();

    connect(ui->action_save, &QAction::triggered, this, &SysInfoView::onSave);
    connect(ui->action_print, &QAction::triggered, this, &SysInfoView::onPrint);
    connect(ui->action_refresh, &QAction::triggered, this, &SysInfoView::onRefresh);

    connect(ui->tree_category, &QTreeWidget::itemClicked,
            this, &SysInfoView::onCategoryItemClicked);

    layout_ = new QHBoxLayout(ui->widget);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(sys_info_widgets_[current_widget_]);
}

//--------------------------------------------------------------------------------------------------
SysInfoView::~SysInfoView() = default;

//--------------------------------------------------------------------------------------------------
QList<QAction*> SysInfoView::fileActions() const
{
    return { ui->action_save, ui->action_print };
}

//--------------------------------------------------------------------------------------------------
QList<QAction*> SysInfoView::viewActions() const
{
    return { ui->action_refresh };
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::setToolBarVisible(bool visible)
{
    ui->toolbar->setVisible(visible);
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::setVersions(const QVersionNumber& client_version,
                              const QVersionNumber& host_version,
                              const QVersionNumber& router_version)
{
    summary_widget_->setClientVersion(client_version);
    summary_widget_->setHostVersion(host_version);
    summary_widget_->setRouterVersion(router_version);
}

//--------------------------------------------------------------------------------------------------
QByteArray SysInfoView::saveState() const
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << ui->splitter->saveState();

        QByteArray widgets_buffer;
        {
            QDataStream widgets_stream(&widgets_buffer, QIODevice::WriteOnly);
            widgets_stream.setVersion(QDataStream::Qt_6_10);

            widgets_stream << quint32(sys_info_widgets_.size());
            for (int i = 0; i < sys_info_widgets_.size(); ++i)
            {
                SysInfoWidget* widget = sys_info_widgets_[i];
                widgets_stream << QByteArray::fromStdString(widget->category());
                widgets_stream << widget->saveState();
            }
        }
        stream << widgets_buffer;
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray value;

    stream >> value;
    if (!value.isEmpty())
        ui->splitter->restoreState(value);

    QByteArray widgets_buffer;
    stream >> widgets_buffer;

    if (widgets_buffer.isEmpty())
        return;

    QDataStream widgets_stream(widgets_buffer);
    widgets_stream.setVersion(QDataStream::Qt_6_10);

    quint32 count = 0;
    widgets_stream >> count;

    QHash<QByteArray, QByteArray> states;
    for (quint32 i = 0; i < count; ++i)
    {
        QByteArray category;
        QByteArray widget_state;

        widgets_stream >> category;
        widgets_stream >> widget_state;

        if (widgets_stream.status() != QDataStream::Ok)
            break;

        states.insert(category, widget_state);
    }

    for (int i = 0; i < sys_info_widgets_.size(); ++i)
    {
        SysInfoWidget* widget = sys_info_widgets_[i];
        QByteArray widget_state = states.value(QByteArray::fromStdString(widget->category()));
        if (!widget_state.isEmpty())
            widget->restoreState(widget_state);
    }
}

//--------------------------------------------------------------------------------------------------
QSize SysInfoView::sizeHint() const
{
    return QSize(kPreferredWidth, kPreferredHeight);
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        SysInfoWidget* widget = sys_info_widgets_[i];

        if (widget->category() == system_info.footer().category())
            widget->setSystemInfo(system_info);
    }

    updateExportActions();
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onRefresh()
{
    for (int i = 0; i < sys_info_widgets_.count(); ++i)
        emit sig_systemInfoRequest(sys_info_widgets_[i]->request());
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onCategoryItemClicked(QTreeWidgetItem* item, int /* column */)
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

    updateExportActions();
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onSave()
{
    QString file_path =
        QFileDialog::getSaveFileName(this, tr("HTML File"), QDir::homePath(), tr("HTML File (*.html)"));
    if (file_path.isEmpty())
        return;

    QString error_string;

    if (!treeToHtmlFile(sys_info_widgets_[current_widget_]->treeWidget(), file_path, &error_string))
        MsgBox::warning(this, tr("Failed to save file: %1").arg(error_string));
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onPrint()
{
    QString html = treeToHtmlString(sys_info_widgets_[current_widget_]->treeWidget());

    QTextDocument document;
    document.setHtml(html);

    QPrinter printer;

    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QPrintDialog::Accepted)
        return;

    document.print(&printer);
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::buildCategoryTree()
{
    CategoryItem* summary_category = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/computer.svg", tr("Summary"), kSystemInfo_Summary);

    //----------------------------------------------------------------------------------------------
    // HARDWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* hardware_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Hardware"));

    CategoryItem* devices = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg", tr("Devices"), kSystemInfo_Devices);

    CategoryItem* drives = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/hdd.svg", tr("Drives"), kSystemInfo_Drives);

    CategoryItem* video_adapters = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/video-card.svg", tr("Video Adapters"), kSystemInfo_VideoAdapters);

    CategoryItem* monitors = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/imac.svg", tr("Monitors"), kSystemInfo_Monitors);

    CategoryItem* printers = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/printer.svg", tr("Printers"), kSystemInfo_Printers);

    CategoryItem* power_options = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/electrical.svg", tr("Power Options"), kSystemInfo_PowerOptions);

    hardware_category->addChild(devices);
    hardware_category->addChild(drives);
    hardware_category->addChild(video_adapters);
    hardware_category->addChild(monitors);
    hardware_category->addChild(printers);
    hardware_category->addChild(power_options);

    //----------------------------------------------------------------------------------------------
    // SOFTWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* software_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Software"));

    CategoryItem* applications = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/software.svg", tr("Applications"), kSystemInfo_Applications);

    CategoryItem* drivers = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg", tr("Drivers"), kSystemInfo_Drivers);

    CategoryItem* services = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/gear.svg", tr("Services"), kSystemInfo_Services);

    CategoryItem* processes = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/heart-monitor.svg", tr("Processes"), kSystemInfo_Processes);

    CategoryItem* licenses = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/certificate.svg", tr("Licenses"), kSystemInfo_Licenses);

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

    CategoryItem* network_adapters = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg", tr("Network Adapters"), kSystemInfo_NetworkAdapters);

    CategoryItem* routes = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/flow-chart.svg", tr("Routes"), kSystemInfo_Routes);

    CategoryItem* connections = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/connected.svg", tr("Connections"), kSystemInfo_Connections);

    CategoryItem* network_shares = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg", tr("Network Shares"), kSystemInfo_NetworkShares);

    CategoryItem* open_files = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg", tr("Open Files"), kSystemInfo_OpenFiles);

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

    CategoryItem* env_vars = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/day-view.svg", tr("Environment Variables"), kSystemInfo_EnvironmentVariables);

    CategoryItem* event_logs = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/log.svg", tr("Event Logs"), kSystemInfo_EventLogs);

    CategoryItem* local_users = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user.svg", tr("Users"), kSystemInfo_LocalUsers);

    CategoryItem* local_user_groups = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user-account.svg", tr("User Groups"), kSystemInfo_LocalUserGroups);

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

    // Restore selection: pick the category that matches the currently shown widget.
    const std::string& current_category = sys_info_widgets_[current_widget_]->category();
    QTreeWidgetItemIterator it(ui->tree_category);
    while (*it)
    {
        CategoryItem* item = static_cast<CategoryItem*>(*it);
        if (item->type() == CategoryItem::Type::CATEGORY_ITEM && item->category() == current_category)
        {
            ui->tree_category->setCurrentItem(item);
            break;
        }
        ++it;
    }
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::updateExportActions()
{
    const bool has_data = sys_info_widgets_[current_widget_]->treeWidget()->topLevelItemCount() != 0;

    ui->action_save->setEnabled(has_data);
    ui->action_print->setEnabled(has_data);
}
