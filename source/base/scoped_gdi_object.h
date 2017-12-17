//
// PROJECT:         Aspia
// FILE:            base/scoped_gdi_object.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_GDI_OBJECT_H
#define _ASPIA_BASE__SCOPED_GDI_OBJECT_H

#include "base/macros.h"

namespace aspia {

// Like ScopedHandle but for GDI objects.
template<class T, class Traits>
class ScopedGDIObject
{
public:
    ScopedGDIObject() = default;

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

    void Reset(T object = nullptr)
    {
        if (object_ && object != object_)
            Traits::Close(object_);
        object_ = object;
    }

    ScopedGDIObject& operator=(T object)
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

    operator T() { return object_; }

private:
    T object_ = nullptr;

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

// Typedefs for some common use cases.
using ScopedHBITMAP = ScopedGDIObject<HBITMAP, DeleteObjectTraits<HBITMAP>>;
using ScopedHRGN = ScopedGDIObject<HRGN, DeleteObjectTraits<HRGN>>;
using ScopedHFONT = ScopedGDIObject<HFONT, DeleteObjectTraits<HFONT>>;
using ScopedHBRUSH = ScopedGDIObject<HBRUSH, DeleteObjectTraits<HBRUSH>>;

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_GDI_OBJECT_H
