//
// PROJECT:         Aspia
// FILE:            system_info/serializer/dmi_serializer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H
#define _ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H

#include "system_info/serializer/serializer.h"

namespace aspia {

class DmiSerializer : public Serializer
{
    Q_OBJECT

public:
    static Serializer* create(QObject* parent, const QString& uuid);

public slots:
    void start() override;

protected:
    DmiSerializer(QObject* parent, const QString& uuid);

private:
    Q_DISABLE_COPY(DmiSerializer)
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H
