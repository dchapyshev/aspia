//
// PROJECT:         Aspia
// FILE:            host/host_notifier.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_notifier.h"

namespace aspia {

HostNotifier::HostNotifier(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostNotifier::~HostNotifier() = default;

} // namespace aspia
