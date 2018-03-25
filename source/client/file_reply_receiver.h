//
// PROJECT:         Aspia
// FILE:            client/file_reply_receiver.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
#define _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H

class QObject;

namespace aspia {

struct FileReplyReceiver
{
    FileReplyReceiver() = default;

    FileReplyReceiver(QObject* object, const char* slot)
        : object(object), slot(slot)
    {
        // Nothing
    }

    QObject* object = nullptr;
    const char* slot = nullptr;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REPLY_RECEIVER_H
