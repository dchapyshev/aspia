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

#ifndef BASE_WIN_SCOPED_USER_OBJECT_H
#define BASE_WIN_SCOPED_USER_OBJECT_H

#include <QtGlobal>
#include <qt_windows.h>

namespace base {

// Like ScopedGDIObject but for User objects.
template<class T, class Traits>
class ScopedUserObject
{
public:
    ScopedUserObject() = default;

    explicit ScopedUserObject(T object)
        : object_(object)
    {
        // Nothing
    }

    ScopedUserObject(ScopedUserObject&& other) noexcept
    {
        object_ = other.object_;
        other.object_ = nullptr;
    }

    ~ScopedUserObject() { Traits::close(object_); }

    T get() { return object_; }

    void reset(T object = nullptr)
    {
        if (object_ && object != object_)
            Traits::close(object_);
        object_ = object;
    }

    T release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    bool isValid() const { return object_ != nullptr; }

    ScopedUserObject& operator=(ScopedUserObject&& other) noexcept
    {
        Traits::close(object_);
        object_ = other.object_;
        other.object_ = nullptr;
        return *this;
    }

    operator T() { return object_; }

private:
    T object_ = nullptr;

    Q_DISABLE_COPY(ScopedUserObject)
};

// The traits class that uses DestroyWindow() to close a handle.
class DestroyWindowTraits
{
public:
    // Closes the handle.
    static void close(HWND handle)
    {
        if (handle)
            DestroyWindow(handle);
    }
};

// The traits class that uses DestroyMenu() to close a handle.
class DestroyMenuTraits
{
public:
    // Closes the handle.
    static void close(HMENU handle)
    {
        if (handle)
            DestroyMenu(handle);
    }
};

// The traits class that uses DestroyCursor() to close a handle.
class DestroyCursorTraits
{
public:
    // Closes the handle.
    static void close(HCURSOR handle)
    {
        if (handle)
            DestroyCursor(handle);
    }
};

// The traits class that uses DestroyIcon() to close a handle.
class DestroyIconTraits
{
public:
    // Closes the handle.
    static void close(HICON handle)
    {
        if (handle)
            DestroyIcon(handle);
    }
};

// The traits class that uses DestroyAcceleratorTable() to close a handle.
class DestroyAccelTraits
{
public:
    // Closes the handle.
    static void close(HACCEL handle)
    {
        if (handle)
            DestroyAcceleratorTable(handle);
    }
};

// The traits class that uses UnhookWindowsHookEx() to close a handle.
class DestroyHookTraits
{
public:
    // Closes the handle.
    static void close(HHOOK handle)
    {
        if (handle)
            UnhookWindowsHookEx(handle);
    }
};

// Typedefs for some common use cases.
using ScopedHWND = ScopedUserObject<HWND, DestroyWindowTraits>;
using ScopedHMENU = ScopedUserObject<HMENU, DestroyMenuTraits>;
using ScopedHICON = ScopedUserObject<HICON, DestroyIconTraits>;
using ScopedHCURSOR = ScopedUserObject<HCURSOR, DestroyCursorTraits>;
using ScopedHACCEL = ScopedUserObject<HACCEL, DestroyAccelTraits>;
using ScopedHHOOK = ScopedUserObject<HHOOK, DestroyHookTraits>;

} // namespace base

#endif // BASE_WIN_SCOPED_USER_OBJECT_H
