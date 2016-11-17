/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/event.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/event.h"

#include "base/exception.h"
#include "base/unicode.h"
#include "base/logging.h"

Event::Event(const char *name) : name_(name)
{
    event_.set(CreateEventW(0, FALSE, FALSE, UNICODEfromUTF8(name_).c_str()));
    if (!event_.get())
    {
        LOG(ERROR) << "CreateEventA() failed: " << GetLastError()
                   << "; Event name: " << name_;

        throw Exception("Unable to create event.");
    }
}

Event::~Event() {}

void Event::Notify()
{
    SetEvent(event_);
}

void Event::WaitForEvent(uint32_t milliseconds)
{
    DWORD error = WaitForSingleObject(event_, milliseconds);

    switch (error)
    {
        case WAIT_OBJECT_0: // Success
            break;

        case WAIT_TIMEOUT:
            LOG(WARNING) << "Waiting timeout for: " << name_;
            break;

        default:
            LOG(WARNING) << "Unknown waiting error for: " << name_ << "; " << error;
            break;
    }
}

void Event::WaitForEvent()
{
    if (WaitForSingleObject(event_, INFINITE) == WAIT_FAILED)
    {
        LOG(WARNING) << "Unknown waiting error for: " << name_;
    }
}
