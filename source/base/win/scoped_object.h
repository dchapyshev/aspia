//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_SCOPED_HANDLE_H
#define BASE_WIN_SCOPED_HANDLE_H

#include <QtGlobal>
#include <qt_windows.h>

namespace base {

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

    void swap(ScopedObject& other) noexcept
    {
        T object = other.object_;
        other.object_ = object_;
        object_ = object;
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

} // namespace base

#endif // BASE_WIN_SCOPED_HANDLE_H
