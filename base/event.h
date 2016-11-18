/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/event.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__EVENT_H
#define _ASPIA_BASE__EVENT_H

#include "aspia_config.h"

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/scoped_handle.h"

class Event
{
public:
    Event();
    virtual ~Event();

    void Notify();

    void WaitForEvent(uint32_t milliseconds);
    void WaitForEvent();

private:
    ScopedHandle event_;

    DISALLOW_COPY_AND_ASSIGN(Event);
};

#endif // _ASPIA_BASE__EVENT_H
