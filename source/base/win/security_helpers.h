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

#ifndef BASE_WIN_SECURITY_HELPERS_H
#define BASE_WIN_SECURITY_HELPERS_H

#include <QString>
#include <qt_windows.h>

#include <cstdint>
#include <memory>

namespace base {

template <typename T>
struct TypedBufferDeleter
{
    void operator()(T* p) const
    {
        delete[] reinterpret_cast<uint8_t*>(p);
    }
};

template <typename T>
using TypedBuffer = std::unique_ptr<T, TypedBufferDeleter<T>>;

template <typename T>
TypedBuffer<T> makeTypedBuffer(size_t length)
{
    if (length == 0)
        return TypedBuffer<T>();

    return TypedBuffer<T>(reinterpret_cast<T*>(new uint8_t[length]));
}

using ScopedAcl = TypedBuffer<ACL>;
using ScopedSd = TypedBuffer<SECURITY_DESCRIPTOR>;
using ScopedSid = TypedBuffer<SID>;

// Initializes COM security of the process applying the passed security
// descriptor.  The function configures the following settings:
//  - Server authenticates that all data received is from the expected client.
//  - Server can impersonate clients to check their identity but cannot act on
//    their behalf.
//  - Caller's identity is verified on every call (Dynamic cloaking).
//  - Unless |activate_as_activator| is true, activations where the server
//    would run under this process's identity are prohibited.
bool initializeComSecurity(const wchar_t* security_descriptor, const wchar_t* mandatory_label,
    bool activate_as_activator);

bool userSidString(QString* user_sid);

ScopedSd convertSddlToSd(const QString& sddl);

} // namespace base

#endif // BASE_WIN_SECURITY_HELPERS_H
