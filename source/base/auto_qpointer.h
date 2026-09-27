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

#ifndef BASE_AUTO_QPOINTER_H
#define BASE_AUTO_QPOINTER_H

#include <QPointer>

// Owning pointer for a QObject. Unlike ScopedQPointer it deletes the object right away when going
// out of scope, and does nothing if the Qt parent has already destroyed it.
template <typename T>
class AutoQPointer
{
public:
    explicit AutoQPointer(T* ptr) noexcept
        : ptr_(ptr)
    {
        // Nothing
    }

    ~AutoQPointer()
    {
        delete ptr_.data();
    }

    T* get() const noexcept { return ptr_.data(); }
    T* operator->() const noexcept { return ptr_.data(); }
    T& operator*() const { return *ptr_; }
    operator T*() const noexcept { return ptr_.data(); }

private:
    QPointer<T> ptr_;

    Q_DISABLE_COPY_MOVE(AutoQPointer)
};

#endif // BASE_AUTO_QPOINTER_H
