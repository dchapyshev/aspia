//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/power_injector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__POWER_INJECTOR_H
#define _ASPIA_BASE__POWER_INJECTOR_H

#include "base/scoped_object.h"
#include "proto/power_session_message.pb.h"

namespace aspia {

bool InjectPowerEvent(const proto::PowerEvent& event);

} // namespace aspia

#endif // _ASPIA_BASE__POWER_INJECTOR_H
