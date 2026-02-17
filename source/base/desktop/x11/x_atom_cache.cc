//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/desktop/x11/x_atom_cache.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
XAtomCache::XAtomCache(::Display* display)
    : display_(display)
{
    DCHECK(display_);
}

//--------------------------------------------------------------------------------------------------
XAtomCache::~XAtomCache() = default;

//--------------------------------------------------------------------------------------------------
::Display* XAtomCache::display() const
{
    return display_;
}

//--------------------------------------------------------------------------------------------------
Atom XAtomCache::wmState()
{
    return createIfNotExist(&wm_state_, "WM_STATE");
}

//--------------------------------------------------------------------------------------------------
Atom XAtomCache::windowType()
{
    return createIfNotExist(&window_type_, "_NET_WM_WINDOW_TYPE");
}

//--------------------------------------------------------------------------------------------------
Atom XAtomCache::windowTypeNormal()
{
    return createIfNotExist(&window_type_normal_, "_NET_WM_WINDOW_TYPE_NORMAL");
}

//--------------------------------------------------------------------------------------------------
Atom XAtomCache::iccProfile()
{
    return createIfNotExist(&icc_profile_, "_ICC_PROFILE");
}

//--------------------------------------------------------------------------------------------------
Atom XAtomCache::createIfNotExist(Atom* atom, const char* name)
{
    DCHECK(atom);

    if (*atom == X11_None)
    {
        *atom = XInternAtom(display(), name, X11_True);
    }

    return *atom;
}

}  // namespace base
