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

#include <QDataStream>
#include <QIODevice>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "client/workers/network_worker.h"
#include "common/sys_info/sys_info_view.h"
#include "proto/peer.h"
#include "proto/system_info.h"

//--------------------------------------------------------------------------------------------------
SystemInfoWindow::SystemInfoWindow(QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_SYSTEM_INFO, parent),
      view_(new SysInfoView(this))
{
    LOG(INFO) << "Ctor";

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
QByteArray SystemInfoWindow::saveState() const
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << saveGeometry();
        stream << view_->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray value;

    stream >> value;
    if (!value.isEmpty())
        restoreGeometry(value);

    stream >> value;
    if (!value.isEmpty())
        view_->restoreState(value);
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
