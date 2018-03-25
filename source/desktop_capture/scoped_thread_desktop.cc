//
// PROJECT:         Aspia
// FILE:            desktop_capture/scoped_thread_desktop.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/scoped_thread_desktop.h"

#include <utility>

namespace aspia {

ScopedThreadDesktop::ScopedThreadDesktop() :
    initial_(Desktop::threadDesktop())
{
    // Nothing
}

ScopedThreadDesktop::~ScopedThreadDesktop()
{
    revert();
}

bool ScopedThreadDesktop::isSame(const Desktop& desktop) const
{
    if (assigned_.isValid())
        return assigned_.isSame(desktop);

    return initial_.isSame(desktop);
}

void ScopedThreadDesktop::revert()
{
    if (assigned_.isValid())
    {
        initial_.setThreadDesktop();
        assigned_.close();
    }
}

bool ScopedThreadDesktop::setThreadDesktop(Desktop desktop)
{
    revert();

    if (initial_.isSame(desktop))
        return true;

    if (!desktop.setThreadDesktop())
        return false;

    assigned_ = std::move(desktop);

    return true;
}

} // namespace aspia
