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

#include "host/android/file_client.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
FileClient::FileClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileClient::~FileClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileClient::onStart()
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void FileClient::onMessage(quint8 /* channel_id */, const QByteArray& /* buffer */)
{
    NOTIMPLEMENTED();
}
