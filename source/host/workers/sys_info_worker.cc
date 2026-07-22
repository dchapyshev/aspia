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

#include "host/workers/sys_info_worker.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "host/system_info.h"
#include "proto/system_info.h"

//--------------------------------------------------------------------------------------------------
SysInfoWorker::SysInfoWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SysInfoWorker::~SysInfoWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::query(QObject* context, const QByteArray& buffer, std::function<void(QByteArray)> reply)
{
    Worker::request(context, [buffer]()
    {
        proto::system_info::SystemInfoRequest request;
        if (!parse(buffer, &request))
        {
            LOG(ERROR) << "Unable to parse system info request";
            return QByteArray();
        }

        proto::system_info::SystemInfo system_info;
        createSystemInfo(request, &system_info);
        return serialize(system_info);
    },
    std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::onStart()
{
    LOG(INFO) << "Sys info worker started";
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::onStop()
{
    LOG(INFO) << "Sys info worker stopped";
}
