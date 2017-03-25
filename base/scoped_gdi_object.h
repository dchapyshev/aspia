//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_gdi_object.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_GDI_OBJECT_H
#define _ASPIA_BASE__SCOPED_GDI_OBJECT_H

#include "aspia_config.h"
#include "base/macros.h"

namespace aspia {

// Like ScopedHandle but for GDI objects.
template<class T, class Traits>
class ScopedGDIObject
{
public:
    ScopedGDIObject() :
        object_(nullptr)
    {
        // Nothing
    }

    explicit ScopedGDIObject(T object) :
        object_(object)
    {
        // Nothing
    }

    ~ScopedGDIObject()
    {
        Traits::Close(object_);
    }

    T Get()
    {
        return object_;
    }

    void Set(T object)
    {
        if (object_ && object != object_)
            Traits::Close(object_);
        object_ = object;
    }

    ScopedGDIObject& operator=(T object)
    {
        Set(object);
        return *this;
    }

    T Release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    operator T() { return object_; }

private:
    T object_;

    DISALLOW_COPY_AND_ASSIGN(ScopedGDIObject);
};

// The traits class that uses DeleteObject() to close a handle.
template <typename T>
class DeleteObjectTraits
{
public:
    // Closes the handle.
    static void Close(T handle)
    {
        if (handle)
            DeleteObject(handle);
    }

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(DeleteObjectTraits);
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

// Typedefs for some common use cases.
typedef ScopedGDIObject<HBITMAP, DeleteObjectTraits<HBITMAP>> ScopedBitmap;
typedef ScopedGDIObject<HRGN, DeleteObjectTraits<HRGN>> ScopedRegion;
typedef ScopedGDIObject<HFONT, DeleteObjectTraits<HFONT>> ScopedHFONT;
typedef ScopedGDIObject<HBRUSH, DeleteObjectTraits<HBRUSH>> ScopedHBRUSH;

typedef ScopedGDIObject<HICON, DestroyIconTraits> ScopedHICON;
typedef ScopedGDIObject<HCURSOR, DestroyCursorTraits> ScopedHCURSOR;

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_GDI_OBJECT_H
