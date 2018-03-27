//
// PROJECT:         Aspia
// FILE:            base/win/scoped_handle.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_HANDLE_H
#define _ASPIA_BASE__WIN__SCOPED_HANDLE_H

#include <QtGlobal>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

template<class T, class Traits>
class ScopedObject
{
public:
    ScopedObject() = default;

    explicit ScopedObject(T object) : object_(object)
    {
        // Nothing
    }

    ScopedObject(ScopedObject&& other) noexcept
    {
        object_ = other.object_;
        other.object_ = nullptr;
    }

    ~ScopedObject()
    {
        Traits::Close(object_);
    }

    T Get() const
    {
        return object_;
    }

    void Reset(T object = nullptr)
    {
        Traits::Close(object_);
        object_ = object;
    }

    T* Recieve()
    {
        Traits::Close(object_);
        return &object_;
    }

    T Release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    bool IsValid() const
    {
        return Traits::IsValid(object_);
    }

    ScopedObject& operator=(T handle)
    {
        Reset(handle);
        return *this;
    }

    ScopedObject& operator=(ScopedObject&& other) noexcept
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

    Q_DISABLE_COPY(ScopedObject)
};

class HandleObjectTraits
{
public:
    // Closes the handle.
    static void Close(HANDLE object)
    {
        if (IsValid(object))
            CloseHandle(object);
    }

    static bool IsValid(HANDLE object)
    {
        return ((object != nullptr) && (object != INVALID_HANDLE_VALUE));
    }
};

class ScHandleObjectTraits
{
public:
    // Closes the handle.
    static void Close(SC_HANDLE object)
    {
        if (IsValid(object))
            CloseServiceHandle(object);
    }

    static bool IsValid(SC_HANDLE object)
    {
        return (object != nullptr);
    }
};

class EventLogObjectTraits
{
public:
    // Closes the handle.
    static void Close(HANDLE object)
    {
        if (IsValid(object))
            CloseEventLog(object);
    }

    static bool IsValid(HANDLE object)
    {
        return (object != nullptr);
    }
};

using ScopedHandle = ScopedObject<HANDLE, HandleObjectTraits>;
using ScopedScHandle = ScopedObject<SC_HANDLE, ScHandleObjectTraits>;
using ScopedEventLog = ScopedObject<HANDLE, EventLogObjectTraits>;

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_HANDLE_H
