//
// PROJECT:         Aspia
// FILE:            base/win/scoped_gdi_object.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_GDI_OBJECT_H
#define _ASPIA_BASE__WIN__SCOPED_GDI_OBJECT_H

#include <qglobal.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

// Like ScopedHandle but for GDI objects.
template<class T, class Traits>
class ScopedGDIObject
{
public:
    ScopedGDIObject() = default;

    explicit ScopedGDIObject(T object)
        : object_(object)
    {
        // Nothing
    }

    ~ScopedGDIObject() { Traits::close(object_); }

    T get() { return object_; }

    void reset(T object = nullptr)
    {
        if (object_ && object != object_)
            Traits::close(object_);
        object_ = object;
    }

    ScopedGDIObject& operator=(T object)
    {
        reset(object);
        return *this;
    }

    T release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    operator T() { return object_; }

private:
    T object_ = nullptr;

    Q_DISABLE_COPY(ScopedGDIObject)
};

// The traits class that uses DeleteObject() to close a handle.
template <typename T>
class DeleteObjectTraits
{
public:
    // Closes the handle.
    static void close(T handle)
    {
        if (handle)
            DeleteObject(handle);
    }
};

// Typedefs for some common use cases.
using ScopedHBITMAP = ScopedGDIObject<HBITMAP, DeleteObjectTraits<HBITMAP>>;
using ScopedHRGN = ScopedGDIObject<HRGN, DeleteObjectTraits<HRGN>>;
using ScopedHFONT = ScopedGDIObject<HFONT, DeleteObjectTraits<HFONT>>;
using ScopedHBRUSH = ScopedGDIObject<HBRUSH, DeleteObjectTraits<HBRUSH>>;

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_GDI_OBJECT_H
