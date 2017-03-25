//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/power_injector.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__POWER_INJECTOR_H
#define _ASPIA_BASE__POWER_INJECTOR_H

#include "aspia_config.h"

#include "base/scoped_handle.h"
#include "proto/proto.pb.h"

namespace aspia {

void InjectPowerEvent(const proto::PowerEvent& event);

} // namespace aspia

#endif // _ASPIA_BASE__POWER_INJECTOR_H
