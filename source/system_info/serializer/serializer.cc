//
// PROJECT:         Aspia
// FILE:            system_info/serializer/serializer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/serializer/serializer.h"

namespace aspia {

Serializer::Serializer(QObject* parent, const QString& uuid)
    : QObject(parent),
      category_uuid_(uuid)
{
    // Nothing
}

void Serializer::onReady(const google::protobuf::MessageLite& message)
{
    QByteArray buffer;

    size_t size = message.ByteSizeLong();
    if (size)
    {
        buffer.resize(size);
        message.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    }

    emit replyReady(requestUuid(), buffer);
    emit finished();
}

} // namespace aspia
