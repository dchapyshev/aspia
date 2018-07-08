//
// PROJECT:         Aspia
// FILE:            host/host_session_system_info.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"

#include <QDebug>
#include <QThread>

#include "base/message_serialization.h"

namespace aspia {

namespace {

enum MessageId { ReplyMessageId };

} // namespace

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

void HostSessionSystemInfo::messageWritten(int message_id)
{
    Q_ASSERT(message_id == ReplyMessageId);
    emit readMessage();
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

    emit writeMessage(ReplyMessageId, serializeMessage(reply));
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

                emit writeMessage(ReplyMessageId, serializeMessage(reply));
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
