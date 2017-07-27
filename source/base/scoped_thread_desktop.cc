//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_thread_desktop.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_thread_desktop.h"

namespace aspia {

ScopedThreadDesktop::ScopedThreadDesktop()
    : initial_(Desktop::GetThreadDesktop())
{
    // Nothing
}

ScopedThreadDesktop::~ScopedThreadDesktop()
{
    Revert();
}

bool ScopedThreadDesktop::IsSame(const Desktop& desktop)
{
    if (assigned_)
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
    if (assigned_)
    {
        initial_->SetThreadDesktop();
        assigned_.reset();
    }
}

bool ScopedThreadDesktop::SetThreadDesktop(Desktop* desktop)
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

} // namespace aspia
