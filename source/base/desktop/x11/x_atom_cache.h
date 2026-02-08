//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_X11_X_ATOM_CACHE_H
#define BASE_DESKTOP_X11_X_ATOM_CACHE_H

#include "base/x11/x11_headers.h"

namespace base {

// A cache of Atom. Each Atom object is created on demand.
class XAtomCache final
{
public:
    explicit XAtomCache(::Display* display);
    ~XAtomCache();

    ::Display* display() const;

    Atom wmState();
    Atom windowType();
    Atom windowTypeNormal();
    Atom iccProfile();

private:
    // If |*atom| is None, this function uses XInternAtom() to retrieve an Atom.
    Atom createIfNotExist(Atom* atom, const char* name);

    ::Display* const display_;
    Atom wm_state_ = X11_None;
    Atom window_type_ = X11_None;
    Atom window_type_normal_ = X11_None;
    Atom icc_profile_ = X11_None;
};

} // namespace base

#endif // BASE_DESKTOP_X11_X_ATOM_CACHE_H
