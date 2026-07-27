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

#include "host/ui/system_info_window.h"

#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "common/sys_info/sys_info_view.h"
#include "host/workers/sys_info_worker.h"
#include "proto/system_info.h"

//--------------------------------------------------------------------------------------------------
SystemInfoWindow::SystemInfoWindow(QWidget* parent)
    : QWidget(parent),
      consumer_id_(SysInfoWorker::createConsumerId()),
      view_(new SysInfoView(this))
{
    LOG(INFO) << "Ctor";

    SysInfoWorker* sys_info_worker = GuiApplication::findWorker<SysInfoWorker>();
    CHECK(sys_info_worker);

    setWindowTitle(tr("System Information"));
    setWindowIcon(QIcon(":/img/system-information.svg"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view_);

    connect(view_, &SysInfoView::sig_systemInfoRequest,
            this, &SystemInfoWindow::onSystemInfoRequest);

    connect(this, &SystemInfoWindow::sig_query,
            sys_info_worker, &SysInfoWorker::onQuery, Qt::QueuedConnection);
    connect(sys_info_worker, &SysInfoWorker::sig_systemInfo,
            this, &SystemInfoWindow::onSystemInfo, Qt::QueuedConnection);

    view_->onRefresh();
}

//--------------------------------------------------------------------------------------------------
SystemInfoWindow::~SystemInfoWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    emit sig_query(consumer_id_, serialize(request));
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindow::onSystemInfo(quint32 consumer_id, const QByteArray& buffer)
{
    if (consumer_id != consumer_id_)
        return;

    proto::system_info::SystemInfo system_info;
    if (!parse(buffer, &system_info))
    {
        LOG(ERROR) << "Unable to parse system info";
        return;
    }

    view_->onSystemInfo(system_info);
}
