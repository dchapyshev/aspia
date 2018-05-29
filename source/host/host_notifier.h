//
// PROJECT:         Aspia
// FILE:            host/host_notifier.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_NOTIFIER_H
#define _ASPIA_HOST__HOST_NOTIFIER_H

#include <QObject>

namespace aspia {

class HostNotifier : public QObject
{
    Q_OBJECT

public:
    HostNotifier(QObject* parent);
    ~HostNotifier();

private:
    Q_DISABLE_COPY(HostNotifier)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_NOTIFIER_H
