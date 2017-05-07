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
    DWORD old_state_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSasPolice);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCOPED_SAS_POLICE_H
