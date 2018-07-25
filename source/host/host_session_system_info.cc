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

#include "host/host_session_system_info.h"

#include <QDebug>
#include <QThread>

#include "base/message_serialization.h"

namespace aspia {

HostSessionSystemInfo::HostSessionSystemInfo(const QString& channel_id)
    : HostSession(channel_id)
{
    // Nothing
}

void HostSessionSystemInfo::messageReceived(const QByteArray& buffer)
{
    proto::system_info::Request request;

    if (!parseMessage(buffer, request))
    {
        emit errorOccurred();
        return;
    }

    if (request.has_category_list_request())
    {
        readCategoryListRequest(request.request_uuid(), request.category_list_request());
    }
    else if (request.has_category_request())
    {
        readCategoryRequest(request.request_uuid(), request.category_request());
    }
    else
    {
        qWarning("Unhandled request");
        emit errorOccurred();
    }
}

void HostSessionSystemInfo::startSession()
{
    category_list_ = Category::all();
}

void HostSessionSystemInfo::stopSession()
{
    category_list_.clear();
}

void HostSessionSystemInfo::readCategoryListRequest(
    const std::string& request_uuid,
    const proto::system_info::CategoryListRequest& /* request */)
{
    proto::system_info::Reply reply;
    reply.set_request_uuid(request_uuid);

    for (const auto& category : category_list_)
        reply.mutable_category_list()->add_uuid(category.uuid().toStdString());

    emit sendMessage(serializeMessage(reply));
}

void HostSessionSystemInfo::readCategoryRequest(
    const std::string& request_uuid,
    const proto::system_info::CategoryRequest& request)
{
    QString category_uuid = QString::fromStdString(request.uuid());

    for (const auto& category : category_list_)
    {
        if (category.uuid() == category_uuid)
        {
            QThread* thread = new QThread(this);

            Serializer* serializer = category.serializer(nullptr);
            serializer->setRequestUuid(QString::fromStdString(request_uuid));
            serializer->setParams(QByteArray::fromStdString(request.params()));
            serializer->moveToThread(thread);

            connect(serializer, &Serializer::finished, thread, &QThread::quit);
            connect(serializer, &Serializer::finished, serializer, &Serializer::deleteLater);
            connect(serializer, &Serializer::replyReady, [&](const QString& request_uuid,
                                                             const QByteArray& data)
            {
                proto::system_info::Reply reply;
                reply.set_request_uuid(request_uuid.toStdString());
                reply.mutable_category()->set_data(data.toStdString());

                emit sendMessage(serializeMessage(reply));
            });

            connect(thread, &QThread::started, serializer, &Serializer::start);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);

            thread->start();
            return;
        }
    }

    qWarning() << "An unknown category was requested: " << category_uuid;
    emit errorOccurred();
}

} // namespace aspia
