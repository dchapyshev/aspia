//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client_session_system_info.h"

#include "base/message_serialization.h"
#include "client/ui/system_info_window.h"

namespace aspia {

ClientSessionSystemInfo::ClientSessionSystemInfo(ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    qRegisterMetaType<proto::system_info::Request>();
    qRegisterMetaType<proto::system_info::Reply>();
}

void ClientSessionSystemInfo::messageReceived(const QByteArray& buffer)
{
    proto::system_info::Reply reply;

    if (!parseMessage(buffer, reply))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    for (auto it = requests_.begin(); it != requests_.end(); ++it)
    {
        SystemInfoRequest* request = it->get();

        if (request->requestUuid() == reply.request_uuid())
        {
            emit request->replyReady(reply);
            requests_.erase(it);
            break;
        }
    }

    writeNextRequest();
}

void ClientSessionSystemInfo::startSession()
{
    window_.reset(new SystemInfoWindow(connect_data_));

    connect(window_.get(), &SystemInfoWindow::newRequest,
            this, &ClientSessionSystemInfo::onNewRequest);

    // When the window is closed, we close the session.
    connect(window_.get(), &SystemInfoWindow::windowClose,
            this, &ClientSessionSystemInfo::closedByUser);

    window_->show();
    window_->activateWindow();
    window_->refresh();

    emit started();
}

void ClientSessionSystemInfo::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(window_.get(), &SystemInfoWindow::windowClose,
               this, &ClientSessionSystemInfo::closedByUser);
    window_->close();
}

void ClientSessionSystemInfo::onNewRequest(SystemInfoRequest* request)
{
    bool schedule_write = requests_.empty();

    requests_.emplace_back(request);

    if (schedule_write)
        writeNextRequest();
}

void ClientSessionSystemInfo::writeNextRequest()
{
    if (requests_.empty())
        return;

    emit sendMessage(serializeMessage(requests_.front()->request()));
}

} // namespace aspia
