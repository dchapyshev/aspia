/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/event.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/event.h"

Event::Event(const std::string &name) : name_(name)
{
    hEvent_ = CreateEventA(0, FALSE, FALSE, name_.c_str());
    if (!hEvent_)
    {
        LOG(ERROR) << "CreateEventA() failed: " << GetLastError();
    }
}

Event::~Event()
{
    CloseHandle(hEvent_);
}

void Event::Notify()
{
    SetEvent(hEvent_);
}

void Event::WaitForEvent(uint32_t milliseconds)
{
    DWORD error = WaitForSingleObject(hEvent_, milliseconds);

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
    if (WaitForSingleObject(hEvent_, INFINITE) == WAIT_FAILED)
    {
        LOG(WARNING) << "Unknown waiting error for: " << name_;
    }
}
