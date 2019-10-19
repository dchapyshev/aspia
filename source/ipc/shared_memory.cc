//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "ipc/shared_memory.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "crypto/random.h"

#include <AclAPI.h>

namespace ipc {

namespace {

std::u16string createFilePath(std::string_view name)
{
    static const char16_t kPrefix[] = u"Global\\aspia_";

    std::u16string result(kPrefix);
    result.append(base::utf16FromAscii(name));

    return result;
}

bool modeToDesiredAccess(SharedMemory::Mode mode, DWORD* desired_access)
{
    switch (mode)
    {
        case SharedMemory::Mode::READ_ONLY:
            *desired_access = FILE_MAP_READ;
            return true;

        case SharedMemory::Mode::READ_WRITE:
            *desired_access = FILE_MAP_WRITE;
            return true;

        default:
            NOTREACHED();
            return false;
    }
}

bool createFileMapping(
    SharedMemory::Mode mode, std::string_view name, size_t size, base::win::ScopedHandle* out)
{
    if (name.empty())
        return false;

    DWORD protect;

    switch (mode)
    {
        case SharedMemory::Mode::READ_ONLY:
            protect = PAGE_READONLY;
            break;

        case SharedMemory::Mode::READ_WRITE:
            protect = PAGE_READWRITE;
            break;

        default:
            NOTREACHED();
            return false;
    }

    const DWORD low = size & 0xFFFFFFFF;
    const DWORD high = static_cast<DWORD64>(size) >> 32 & 0xFFFFFFFF;

    std::u16string path = createFilePath(name);

    base::win::ScopedHandle file(CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, protect, high, low, base::asWide(path)));
    if (!file.isValid())
    {
        PLOG(LS_WARNING) << "CreateFileMappingW failed";
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LOG(LS_WARNING) << "Already exists shared memory: " << path;
        return false;
    }

    DWORD error_code = SetSecurityInfo(
        file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, nullptr);
    if (error_code != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "SetSecurityInfo failed: " << base::systemErrorCodeToString(error_code);
        return false;
    }

    out->swap(file);
    return true;
}

bool openFileMapping(SharedMemory::Mode mode, std::string_view name, base::win::ScopedHandle* out)
{
    if (name.empty())
        return false;

    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    base::win::ScopedHandle file(
        OpenFileMappingW(desired_access, FALSE, base::asWide(createFilePath(name))));
    if (!file.isValid())
    {
        PLOG(LS_WARNING) << "OpenFileMappingW failed";
        return false;
    }

    out->swap(file);
    return true;
}

bool mapViewOfFile(SharedMemory::Mode mode, HANDLE file, void** memory)
{
    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    *memory = MapViewOfFile(file, desired_access, 0, 0, 0);
    if (!*memory)
    {
        PLOG(LS_WARNING) << "MapViewOfFile failed";
        return false;
    }

    return true;
}

} // namespace

SharedMemory::SharedMemory(base::win::ScopedHandle&& file, void* memory)
    : file_(std::move(file)),
      memory_(memory)
{
    // Nothing
}

SharedMemory::~SharedMemory()
{
    if (memory_)
        UnmapViewOfFile(memory_);
}

// static
std::unique_ptr<SharedMemory> SharedMemory::create(Mode mode, std::string_view name, size_t size)
{
    base::win::ScopedHandle file;
    if (!createFileMapping(mode, name, size, &file))
        return nullptr;

    void* memory;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    memset(memory, 0, size);

    return std::unique_ptr<SharedMemory>(new SharedMemory(std::move(file), memory));
}

// static
std::unique_ptr<SharedMemory> SharedMemory::map(Mode mode, std::string_view name)
{
    base::win::ScopedHandle file;
    if (!openFileMapping(mode, name, &file))
        return nullptr;

    void* memory;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    return std::unique_ptr<SharedMemory>(new SharedMemory(std::move(file), memory));
}

// static
std::string SharedMemory::createUniqueName()
{
    return base::numberToString(GetCurrentProcessId()) +
           base::numberToString(crypto::Random::number64()) +
           base::numberToString(crypto::Random::number64());
}

} // namespace ipc
