//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/monitor_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__MONITOR_ENUMERATOR_H
#define _ASPIA_BASE__DEVICES__MONITOR_ENUMERATOR_H

#include "base/devices/device_enumerator.h"
#include "base/devices/edid_parser.h"

#include <memory>

namespace aspia {

class MonitorEnumerator : public DeviceEnumerator
{
public:
    MonitorEnumerator();
    ~MonitorEnumerator() = default;

    std::unique_ptr<EdidParser> GetEDID() const;
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__MONITOR_ENUMERATOR_H
