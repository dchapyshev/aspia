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

#include "client/desktop/sys_info/system_info_window.h"

#include <QVBoxLayout>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "client/workers/network_worker.h"
#include "common/sys_info/sys_info_view.h"
#include "proto/peer.h"
#include "proto/system_info.h"

namespace {

const char kGeometryKey[] = "geometry";
const char kViewKey[] = "view";

} // namespace

//--------------------------------------------------------------------------------------------------
SystemInfoWindow::SystemInfoWindow(QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_SYSTEM_INFO, parent),
      view_(new SysInfoView(this)),
      event_logs_(view_->findChild<SysInfoWidgetEventLogs*>())
{
    LOG(INFO) << "Ctor";
    CHECK(event_logs_);

    setWindowTitle(tr("System Information"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view_);

    connect(view_, &SysInfoView::sig_systemInfoRequest,
            this, &SystemInfoWindow::onSystemInfoRequest);
}

//--------------------------------------------------------------------------------------------------
SystemInfoWindow::~SystemInfoWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::setTabbedMode(bool tabbed)
{
    LOG(INFO) << "Set tabbed mode:" << tabbed;
    view_->setToolBarVisible(!tabbed);
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> SystemInfoWindow::tabActionGroups() const
{
    return {
        { Tab::ActionRole::FILE, view_->fileActions() },
        { Tab::ActionRole::VIEW, view_->viewActions() },
        { Tab::ActionRole::ACTION, sessionConnectActions() }
    };
}

//--------------------------------------------------------------------------------------------------
QVariantMap SystemInfoWindow::saveState() const
{
    QVariantMap state;
    state.insert(kGeometryKey, saveGeometry());
    state.insert(kViewKey, view_->saveState());
    return state;
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::restoreState(const QVariantMap& state)
{
    const QByteArray geometry = state.value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QVariantMap view_state = state.value(kViewKey).toMap();
    if (!view_state.isEmpty())
        view_->restoreState(view_state);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onInternalReset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onRegisterWorkers()
{
    connect(networkWorker(), &NetworkWorker::sig_channel_0,
            this, &SystemInfoWindow::onChannelMessage, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onSessionStarted()
{
    LOG(INFO) << "System info session started";

    view_->setVersions(kCurrentVersion,
                       sessionState()->hostVersion(),
                       sessionState()->routerVersion());

    show();
    activateWindow();

    view_->onRefresh();
}

//--------------------------------------------------------------------------------------------------
QList<SysInfoView::Page> SystemInfoWindow::mcpPages() const
{
    return view_->pages();
}

//--------------------------------------------------------------------------------------------------
bool SystemInfoWindow::mcpShowPage(const QString& key)
{
    return view_->showPage(key);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::mcpBuildReport(SysInfoReport* report)
{
    view_->buildReport(report);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::mcpRefresh()
{
    view_->onRefresh();
}

//--------------------------------------------------------------------------------------------------
proto::system_info::EventLogs::Event::Type SystemInfoWindow::mcpCurrentEventLog() const
{
    return event_logs_->currentLog();
}

//--------------------------------------------------------------------------------------------------
bool SystemInfoWindow::mcpCanMoveEventLog(SysInfoWidgetEventLogs::Move to) const
{
    return event_logs_->canMove(to);
}

//--------------------------------------------------------------------------------------------------
bool SystemInfoWindow::mcpSelectEventLog(proto::system_info::EventLogs::Event::Type type)
{
    return event_logs_->selectLog(type);
}

//--------------------------------------------------------------------------------------------------
bool SystemInfoWindow::mcpMoveEventLog(SysInfoWidgetEventLogs::Move to)
{
    return event_logs_->move(to);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onChannelMessage(const QByteArray& buffer)
{
    proto::system_info::SystemInfo system_info;
    if (!parse(buffer, &system_info))
    {
        LOG(ERROR) << "Unable to parse system info";
        return;
    }

    view_->onSystemInfo(system_info);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(request));
}
