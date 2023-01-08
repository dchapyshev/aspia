//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <X11/X.h>
#include <X11/Xlib.h>

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
    Atom wm_state_ = None;
    Atom window_type_ = None;
    Atom window_type_normal_ = None;
    Atom icc_profile_ = None;
};

} // namespace base

#endif // BASE_DESKTOP_X11_X_ATOM_CACHE_H
