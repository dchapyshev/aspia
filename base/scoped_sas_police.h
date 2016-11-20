/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_sas_police.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_SAS_POLICE_H
#define _ASPIA_BASE__SCOPED_SAS_POLICE_H

#include "aspia_config.h"

class ScopedSasPolice
{
public:
    ScopedSasPolice();
    ~ScopedSasPolice();

private:
    void Close();

private:
    HKEY key_;
    DWORD old_state_;
};

#endif // _ASPIA_BASE__SCOPED_SAS_POLICE_H
