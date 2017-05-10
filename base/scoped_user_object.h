//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_user_object.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_USER_OBJECT_H
#define _ASPIA_BASE__SCOPED_USER_OBJECT_H

#include "base/macros.h"

namespace aspia {

// Like ScopedGDIObject but for User objects.
template<class T, class Traits>
class ScopedUserObject
{
public:
    ScopedUserObject() = default;

    explicit ScopedUserObject(T object) :
        object_(object)
    {
        // Nothing
    }

    ScopedUserObject(ScopedUserObject&& other)
    {
        object_ = other.object_;
        other.object_ = nullptr;
    }

    ~ScopedUserObject()
    {
        Traits::Close(object_);
    }

    T Get()
    {
        return object_;
    }

    void Reset(T object = nullptr)
    {
        if (object_ && object != object_)
            Traits::Close(object_);
        object_ = object;
    }

    ScopedUserObject& operator=(T object)
    {
        Reset(object);
        return *this;
    }

    T Release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    bool IsValid() const
    {
        return object_ != nullptr;
    }

    ScopedUserObject& operator=(ScopedUserObject&& other)
    {
        Traits::Close(object_);
        object_ = other.object_;
        other.object_ = nullptr;
        return *this;
    }

    operator T()
    {
        return object_;
    }

private:
    T object_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedUserObject);
};

// The traits class that uses DestroyWindow() to close a handle.
class DestroyWindowTraits
{
public:
    // Closes the handle.
    static void Close(HWND handle)
    {
        if (handle)
            DestroyWindow(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyWindowTraits);
};

// The traits class that uses DestroyMenu() to close a handle.
class DestroyMenuTraits
{
public:
    // Closes the handle.
    static void Close(HMENU handle)
    {
        if (handle)
            DestroyMenu(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyMenuTraits);
};

// The traits class that uses DestroyCursor() to close a handle.
class DestroyCursorTraits
{
public:
    // Closes the handle.
    static void Close(HCURSOR handle)
    {
        if (handle)
            DestroyCursor(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyCursorTraits);
};

// The traits class that uses DestroyIcon() to close a handle.
class DestroyIconTraits
{
public:
    // Closes the handle.
    static void Close(HICON handle)
    {
        if (handle)
            DestroyIcon(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyIconTraits);
};

// The traits class that uses DestroyAcceleratorTable() to close a handle.
class DestroyAccelTraits
{
public:
    // Closes the handle.
    static void Close(HACCEL handle)
    {
        if (handle)
            DestroyAcceleratorTable(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyAccelTraits);
};

// The traits class that uses UnhookWindowsHookEx() to close a handle.
class DestroyHookTraits
{
public:
    // Closes the handle.
    static void Close(HHOOK handle)
    {
        if (handle)
            UnhookWindowsHookEx(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DestroyHookTraits);
};

// Typedefs for some common use cases.
using ScopedHWND = ScopedUserObject<HWND, DestroyWindowTraits>;
using ScopedHMENU = ScopedUserObject<HMENU, DestroyMenuTraits>;
using ScopedHICON = ScopedUserObject<HICON, DestroyIconTraits>;
using ScopedHCURSOR = ScopedUserObject<HCURSOR, DestroyCursorTraits>;
using ScopedHACCEL = ScopedUserObject<HACCEL, DestroyAccelTraits>;
using ScopedHHOOK = ScopedUserObject<HHOOK, DestroyHookTraits>;

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_USER_OBJECT_H
