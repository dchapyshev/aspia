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

#include "base/logging.h"
#include "base/serialization.h"

#if defined(Q_OS_WINDOWS)
#include "host/system_info.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
SystemInfoClient::SystemInfoClient(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
SystemInfoClient::~SystemInfoClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::start()
{
    tcp_channel_->resume();
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(WARNING) << "TCP error occurred:" << error_code;

    tcp_channel_->disconnect(this);
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClient::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
#if defined(Q_OS_WINDOWS)
    proto::system_info::SystemInfoRequest request;

    if (!base::parse(buffer, &request))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(request, &system_info);

    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, base::serialize(system_info));
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
