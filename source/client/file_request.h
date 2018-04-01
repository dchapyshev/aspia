//
// PROJECT:         Aspia
// FILE:            client/file_request.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REQUEST_H
#define _ASPIA_CLIENT__FILE_REQUEST_H

#include <QObject>
#include <QString>
#include <memory>

#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileRequest : public QObject
{
    Q_OBJECT

public:
    const proto::file_transfer::Request& request() const { return request_; }
    bool sendReply(const proto::file_transfer::Reply& reply);

    static FileRequest* driveListRequest(QObject* sender, const char* reply_slot);

    static FileRequest* fileListRequest(QObject* sender,
                                        const QString& path,
                                        const char* reply_slot);

    static FileRequest* createDirectoryRequest(QObject* sender,
                                               const QString& path,
                                               const char* reply_slot);

    static FileRequest* renameRequest(QObject* sender,
                                      const QString& old_name,
                                      const QString& new_name,
                                      const char* reply_slot);

    static FileRequest* removeRequest(QObject* sender,
                                      const QString& path,
                                      const char* reply_slot);

    static FileRequest* downloadRequest(QObject* sender,
                                        const QString& file_path,
                                        const char* reply_slot);

    static FileRequest* uploadRequest(QObject* sender,
                                      const QString& file_path,
                                      bool overwrite,
                                      const char* reply_slot);

    static FileRequest* packetRequest(QObject* sender, const char* reply_slot);

    static FileRequest* packet(QObject* sender,
                               const proto::file_transfer::Packet& packet,
                               const char* reply_slot);

private slots:
    void senderDestroyed();

private:
    FileRequest(QObject* sender,
                proto::file_transfer::Request&& request,
                const char* reply_slot);

    QObject* sender_;
    proto::file_transfer::Request request_;
    const char* reply_slot_;

    Q_DISABLE_COPY(FileRequest)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REQUEST_H
