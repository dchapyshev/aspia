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

#include "host/system_info_client.h"

#include "base/core_application.h"
#include "base/logging.h"
#include "host/workers/sys_info_worker.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
SystemInfoClient::SystemInfoClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent)
{
    CLOG(INFO) << "Ctor";

    SysInfoWorker* sys_info_worker = CoreApplication::findWorker<SysInfoWorker>();
    CCHECK(sys_info_worker);

    connect(this, &SystemInfoClient::sig_query,
            sys_info_worker, &SysInfoWorker::onQuery, Qt::QueuedConnection);
    connect(sys_info_worker, &SysInfoWorker::sig_systemInfo,
            this, &SystemInfoClient::onSystemInfo, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
SystemInfoClient::~SystemInfoClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::onStart()
{
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::onMessage(quint8 /* channel_id */, const QByteArray& buffer)
{
    // Building the report can take a while; offload it to the dedicated worker and send the
    // result once it arrives with sig_systemInfo.
    emit sig_query(buffer);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::onSystemInfo(const QByteArray& buffer)
{
    send(proto::peer::CHANNEL_ID_0, buffer);
}
