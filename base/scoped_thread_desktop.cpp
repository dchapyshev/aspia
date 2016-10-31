/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_thread_desktop.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/scoped_thread_desktop.h"

ScopedThreadDesktop::ScopedThreadDesktop() :
    initial_(Desktop::GetThreadDesktop())
{
}

ScopedThreadDesktop::~ScopedThreadDesktop()
{
    Revert();
}

bool ScopedThreadDesktop::IsSame(const Desktop& desktop)
{
    if (assigned_.get())
    {
        return assigned_->IsSame(desktop);
    }
    else
    {
        return initial_->IsSame(desktop);
    }
}

void ScopedThreadDesktop::Revert()
{
    if (assigned_.get())
    {
        initial_->SetThreadDesktop();
        assigned_.reset();
    }
}

bool ScopedThreadDesktop::SetThreadDesktop(Desktop *desktop)
{
    Revert();

    std::unique_ptr<Desktop> scoped_desktop(desktop);

    if (initial_->IsSame(*desktop))
        return true;

    if (!desktop->SetThreadDesktop())
        return false;

    assigned_.reset(scoped_desktop.release());

    return true;
}
