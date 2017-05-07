//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_thread_desktop.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_thread_desktop.h"

namespace aspia {

ScopedThreadDesktop::ScopedThreadDesktop() :
    initial_(Desktop::GetThreadDesktop())
{
    // Nothing
}

ScopedThreadDesktop::~ScopedThreadDesktop()
{
    Revert();
}

bool ScopedThreadDesktop::IsSame(const Desktop& desktop)
{
    if (assigned_.IsValid())
    {
        return assigned_.IsSame(desktop);
    }

    return initial_.IsSame(desktop);
}

void ScopedThreadDesktop::Revert()
{
    if (assigned_.IsValid())
    {
        initial_.SetThreadDesktop();
        assigned_.Close();
    }
}

bool ScopedThreadDesktop::SetThreadDesktop(Desktop desktop)
{
    Revert();

    if (initial_.IsSame(desktop))
        return true;

    if (!desktop.SetThreadDesktop())
        return false;

    assigned_ = std::move(desktop);

    return true;
}

} // namespace aspia
