//
// PROJECT:         Aspia
// FILE:            host/system_info_request.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SYSTEM_INFO_REQUEST_H
#define _ASPIA_HOST__SYSTEM_INFO_REQUEST_H

#include <QObject>
#include <QPointer>
#include <QString>

#include "protocol/system_info_session.pb.h"

namespace aspia {

class SystemInfoRequest
{
public:
    const proto::system_info::Request& request() const { return request_; }
    bool sendReply(const proto::system_info::Reply& reply);

    QString requestUuid() const;

    static SystemInfoRequest* categoryList(QObject* sender, const char* reply_slot);

    static SystemInfoRequest* category(QObject* sender,
                                       const QString& uuid,
                                       const QByteArray& params,
                                       const char* reply_slot);

private:
    SystemInfoRequest(QObject* sender,
                      proto::system_info::Request&& request,
                      const char* reply_slot);

    QPointer<QObject> sender_;
    proto::system_info::Request request_;
    const char* reply_slot_;

    Q_DISABLE_COPY(SystemInfoRequest)
};

} // namespace aspia

#endif // _ASPIA_HOST__SYSTEM_INFO_REQUEST_H
