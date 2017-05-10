//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/scoped_sas_police.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SCOPED_SAS_POLICE_H
#define _ASPIA_HOST__SCOPED_SAS_POLICE_H

#include "base/macros.h"
#include "base/registry.h"

namespace aspia {

class ScopedSasPolice
{
public:
    ScopedSasPolice();
    ~ScopedSasPolice();

private:
    RegistryKey key_;

    enum State
    {
        STATE_NONE = 0,
        STATE_SERVICES = 1,
        STATE_APPLICATIONS = 2,
        STATE_SERVICES_AND_APPLICATIONS = 3
    };

    DWORD old_state_ = STATE_NONE;

    DISALLOW_COPY_AND_ASSIGN(ScopedSasPolice);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCOPED_SAS_POLICE_H
