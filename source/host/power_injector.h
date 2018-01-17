//
// PROJECT:         Aspia
// FILE:            base/power_injector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__POWER_INJECTOR_H
#define _ASPIA_BASE__POWER_INJECTOR_H

#include "proto/power_session.pb.h"

namespace aspia {

bool InjectPowerCommand(proto::power::Command command);

} // namespace aspia

#endif // _ASPIA_BASE__POWER_INJECTOR_H
