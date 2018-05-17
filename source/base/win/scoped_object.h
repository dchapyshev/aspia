//
// PROJECT:         Aspia
// FILE:            base/win/scoped_handle.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_HANDLE_H
#define _ASPIA_BASE__WIN__SCOPED_HANDLE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

template<class T, class Traits>
class ScopedObject
{
public:
    ScopedObject() = default;

    explicit ScopedObject(T object)
        : object_(object)
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
        Traits::close(object_);
    }

    T get() const
    {
        return object_;
    }

    void reset(T object = nullptr)
    {
        Traits::close(object_);
        object_ = object;
    }

    T* recieve()
    {
        Traits::close(object_);
        return &object_;
    }

    T release()
    {
        T object = object_;
        object_ = nullptr;
        return object;
    }

    bool isValid() const
    {
        return Traits::isValid(object_);
    }

    ScopedObject& operator=(ScopedObject&& other) noexcept
    {
        Traits::close(object_);
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
    static void close(HANDLE object)
    {
        if (isValid(object))
            CloseHandle(object);
    }

    static bool isValid(HANDLE object)
    {
        return ((object != nullptr) && (object != INVALID_HANDLE_VALUE));
    }
};

class ScHandleObjectTraits
{
public:
    // Closes the handle.
    static void close(SC_HANDLE object)
    {
        if (isValid(object))
            CloseServiceHandle(object);
    }

    static bool isValid(SC_HANDLE object)
    {
        return (object != nullptr);
    }
};

class EventLogObjectTraits
{
public:
    // Closes the handle.
    static void close(HANDLE object)
    {
        if (isValid(object))
            CloseEventLog(object);
    }

    static bool isValid(HANDLE object)
    {
        return (object != nullptr);
    }
};

using ScopedHandle = ScopedObject<HANDLE, HandleObjectTraits>;
using ScopedScHandle = ScopedObject<SC_HANDLE, ScHandleObjectTraits>;
using ScopedEventLog = ScopedObject<HANDLE, EventLogObjectTraits>;

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_HANDLE_H
