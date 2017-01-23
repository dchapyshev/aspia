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
template<class T>
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
        Close();
    }

    T Get()
    {
        return object_;
    }

    void Set(T object)
    {
        if (object_ && object != object_)
            Close();
        object_ = object;
    }

    ScopedGDIObject& operator=(T object)
    {
        Set(object);
        return *this;
    }

    T release()
    {
        T object = object_;
        object_ = NULL;
        return object;
    }

    operator T() { return object_; }

private:
    void Close()
    {
        if (object_)
            DeleteObject(object_);
    }

    T object_;

    DISALLOW_COPY_AND_ASSIGN(ScopedGDIObject);
};

//
// An explicit specialization for HICON because we have to call DestroyIcon()
// instead of DeleteObject() for HICON.
//
template<>
void ScopedGDIObject<HICON>::Close()
{
    if (object_)
        DestroyIcon(object_);
}

// Typedefs for some common use cases.
typedef ScopedGDIObject<HBITMAP> ScopedBitmap;
typedef ScopedGDIObject<HRGN> ScopedRegion;
typedef ScopedGDIObject<HFONT> ScopedHFONT;
typedef ScopedGDIObject<HICON> ScopedHICON;
typedef ScopedGDIObject<HBRUSH> ScopedHBRUSH;

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_GDI_OBJECT_H
