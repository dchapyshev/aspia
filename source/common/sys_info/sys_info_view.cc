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
#include <QHBoxLayout>
#include <QIODevice>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVersionNumber>

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "common/desktop/msg_box.h"
#include "common/sys_info/sys_info_report.h"
#include "common/sys_info/sys_info_widget_applications.h"
#include "common/sys_info/sys_info_widget_connections.h"
#include "common/sys_info/sys_info_widget_cpu.h"
#include "common/sys_info/sys_info_widget_devices.h"
#include "common/sys_info/sys_info_widget_dmi.h"
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
#include "common/sys_info/sys_info_widget_smart.h"
#include "common/sys_info/sys_info_widget_summary.h"
#include "common/sys_info/sys_info_widget_video_adapters.h"
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

    CategoryItem(Type type, const QString& icon_path, const QString& text,
                 SysInfoWidget* page = nullptr)
        : type_(type),
          page_(page)
    {
        setIcon(0, QIcon(icon_path));
        setText(0, text);
    }

    Type type() const { return type_; }

    // Page the item shows. The same report is shown by more than one page, so the item names the
    // page itself and not the category of the report.
    SysInfoWidget* page() const { return page_; }

private:
    Type type_;
    SysInfoWidget* page_;

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
    sys_info_widgets_.append(new SysInfoWidgetCpu(this));
    sys_info_widgets_.append(new SysInfoWidgetDevices(this));
    sys_info_widgets_.append(new SysInfoWidgetDrives(this));
    sys_info_widgets_.append(new SysInfoWidgetSmart(this));
    sys_info_widgets_.append(new SysInfoWidgetVideoAdapters(this));
    sys_info_widgets_.append(new SysInfoWidgetMonitors(this));
    sys_info_widgets_.append(new SysInfoWidgetNetAdapters(this));
    sys_info_widgets_.append(new SysInfoWidgetNetShares(this));
    sys_info_widgets_.append(new SysInfoWidgetPowerOptions(this));
    sys_info_widgets_.append(new SysInfoWidgetDmi(this));
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

    // The category follows the current item, so the keyboard changes the page as the mouse does.
    connect(ui->tree_category, &QTreeWidget::currentItemChanged,
            this, &SysInfoView::onCategoryItemChanged);

    layout_ = new QHBoxLayout(ui->widget);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(sys_info_widgets_[current_widget_]);

    // The page stays empty until the first report arrives.
    updateExportActions();
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

            // The state of a page is stored by its position: more than one page is built from the
            // same report, so the category names no single page.
            widgets_stream << quint32(sys_info_widgets_.size());
            for (int i = 0; i < sys_info_widgets_.size(); ++i)
                widgets_stream << sys_info_widgets_[i]->saveState();
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

    count = std::min(count, quint32(sys_info_widgets_.size()));

    for (quint32 i = 0; i < count; ++i)
    {
        QByteArray widget_state;
        widgets_stream >> widget_state;

        if (widgets_stream.status() != QDataStream::Ok)
            break;

        if (!widget_state.isEmpty())
            sys_info_widgets_[i]->restoreState(widget_state);
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
    SysInfoWidgetSummary* summary = findChild<SysInfoWidgetSummary*>();

    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        SysInfoWidget* widget = sys_info_widgets_[i];

        if (widget == summary || widget->category() == system_info.header().category())
            widget->setSystemInfo(system_info);
    }

    updateExportActions();
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onRefresh()
{
    // The pages built from the same report ask the other side for it once.
    std::set<std::string> requested;

    for (int i = 0; i < sys_info_widgets_.count(); ++i)
    {
        const std::vector<proto::system_info::SystemInfoRequest> requests =
            sys_info_widgets_[i]->requests();

        for (const proto::system_info::SystemInfoRequest& request : requests)
        {
            if (requested.insert(request.category()).second)
                emit sig_systemInfoRequest(request);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onCategoryItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
{
    CategoryItem* category_item = static_cast<CategoryItem*>(current);
    if (!category_item)
        return;

    if (category_item->type() == CategoryItem::Type::ROOT_ITEM)
        return;

    const int index = sys_info_widgets_.indexOf(category_item->page());
    if (index < 0)
        return;

    layout_->removeWidget(sys_info_widgets_[current_widget_]);
    sys_info_widgets_[current_widget_]->setVisible(false);

    current_widget_ = index;

    SysInfoWidget* widget = sys_info_widgets_[index];

    LOG(INFO) << "Current category changed:" << widget->category() << "(" << index << ")";

    layout_->addWidget(widget);
    widget->setVisible(true);

    updateExportActions();
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onSave()
{
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("HTML File"), QDir::homePath(), tr("HTML File (*.html)"));
    if (file_path.isEmpty())
        return;

    SysInfoReport report;
    sys_info_widgets_[current_widget_]->buildReport(&report);

    QString error_string;

    if (!report.save(file_path, &error_string))
        MsgBox::warning(this, tr("Failed to save file: %1").arg(error_string));
}

//--------------------------------------------------------------------------------------------------
void SysInfoView::onPrint()
{
    SysInfoReport report;
    sys_info_widgets_[current_widget_]->buildReport(&report);

    QTextDocument document;
    document.setHtml(report.toString());

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
        ":/img/computer.svg", tr("Summary"), findChild<SysInfoWidgetSummary*>());

    //----------------------------------------------------------------------------------------------
    // HARDWARE
    //----------------------------------------------------------------------------------------------

    CategoryItem* hardware_category = new CategoryItem(
        CategoryItem::Type::ROOT_ITEM, ":/img/folder.svg", tr("Hardware"));

    CategoryItem* dmi = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/motherboard.svg", tr("DMI"), findChild<SysInfoWidgetDmi*>());

    CategoryItem* cpu = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/microchip.svg", tr("Processor"), findChild<SysInfoWidgetCpu*>());

    CategoryItem* devices = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg", tr("Devices"), findChild<SysInfoWidgetDevices*>());

    CategoryItem* drives = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/hdd.svg", tr("Drives"), findChild<SysInfoWidgetDrives*>());

    CategoryItem* smart = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/hdd.svg", tr("S.M.A.R.T."), findChild<SysInfoWidgetSmart*>());

    CategoryItem* video_adapters = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/video-card.svg", tr("Video Adapters"), findChild<SysInfoWidgetVideoAdapters*>());

    CategoryItem* monitors = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/imac.svg", tr("Monitors"), findChild<SysInfoWidgetMonitors*>());

    CategoryItem* printers = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/printer.svg", tr("Printers"), findChild<SysInfoWidgetPrinters*>());

    CategoryItem* power_options = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/electrical.svg", tr("Power Options"), findChild<SysInfoWidgetPowerOptions*>());

    hardware_category->addChild(dmi);
    hardware_category->addChild(cpu);
    hardware_category->addChild(devices);
    hardware_category->addChild(drives);
    hardware_category->addChild(smart);
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
        ":/img/software.svg", tr("Applications"), findChild<SysInfoWidgetApplications*>());

    CategoryItem* drivers = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/network-card.svg", tr("Drivers"), findChild<SysInfoWidgetDrivers*>());

    CategoryItem* services = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/gear.svg", tr("Services"), findChild<SysInfoWidgetServices*>());

    CategoryItem* processes = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/heart-monitor.svg", tr("Processes"), findChild<SysInfoWidgetProcesses*>());

    CategoryItem* licenses = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/certificate.svg", tr("Licenses"), findChild<SysInfoWidgetLicenses*>());

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
        ":/img/network-card.svg", tr("Network Adapters"), findChild<SysInfoWidgetNetAdapters*>());

    CategoryItem* routes = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/flow-chart.svg", tr("Routes"), findChild<SysInfoWidgetRoutes*>());

    CategoryItem* connections = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/connected.svg", tr("Connections"), findChild<SysInfoWidgetConnections*>());

    CategoryItem* network_shares = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg", tr("Network Shares"), findChild<SysInfoWidgetNetShares*>());

    CategoryItem* open_files = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/nas.svg", tr("Open Files"), findChild<SysInfoWidgetOpenFiles*>());

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
        ":/img/day-view.svg", tr("Environment Variables"), findChild<SysInfoWidgetEnvVars*>());

    CategoryItem* event_logs = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/log.svg", tr("Event Logs"), findChild<SysInfoWidgetEventLogs*>());

    CategoryItem* local_users = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user.svg", tr("Users"), findChild<SysInfoWidgetLocalUsers*>());

    CategoryItem* local_user_groups = new CategoryItem(CategoryItem::Type::CATEGORY_ITEM,
        ":/img/user-account.svg", tr("User Groups"), findChild<SysInfoWidgetLocalUserGroups*>());

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
    SysInfoWidget* current_page = sys_info_widgets_[current_widget_];
    QTreeWidgetItemIterator it(ui->tree_category);
    while (*it)
    {
        CategoryItem* item = static_cast<CategoryItem*>(*it);
        if (item->page() == current_page)
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
    const QTreeWidget* tree = sys_info_widgets_[current_widget_]->treeWidget();
    const bool has_data = tree && tree->topLevelItemCount();

    ui->action_save->setEnabled(has_data);
    ui->action_print->setEnabled(has_data);
}
