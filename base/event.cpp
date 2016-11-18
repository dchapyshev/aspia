/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/event.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/event.h"

#include "base/logging.h"

Event::Event()
{
    event_.set(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!event_.get())
    {
        LOG(FATAL) << "CreateEventW() failed: " << GetLastError();
    }
}

Event::~Event()
{
    // Nothing
}

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
            LOG(WARNING) << "Waiting timeout";
            break;

        default:
            LOG(WARNING) << "Unknown waiting error: " << error;
            break;
    }
}

void Event::WaitForEvent()
{
    if (WaitForSingleObject(event_, INFINITE) == WAIT_FAILED)
    {
        LOG(WARNING) << "Unknown waiting error";
    }
}
