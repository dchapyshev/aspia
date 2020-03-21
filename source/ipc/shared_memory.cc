//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "ipc/shared_memory_factory_proxy.h"

#include <random>

#include <AclAPI.h>

namespace ipc {

namespace {

std::u16string createFilePath(int id)
{
    static const char16_t kPrefix[] = u"Global\\aspia_";
    return kPrefix + base::numberToString16(id);
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

bool createFileMapping(SharedMemory::Mode mode, int id, size_t size, base::win::ScopedHandle* out)
{
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

    std::u16string path = createFilePath(id);

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
        LOG(LS_WARNING) << "SetSecurityInfo failed: " << base::SystemError::toString(error_code);
        return false;
    }

    out->swap(file);
    return true;
}

bool openFileMapping(SharedMemory::Mode mode, int id, base::win::ScopedHandle* out)
{
    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    base::win::ScopedHandle file(
        OpenFileMappingW(desired_access, FALSE, base::asWide(createFilePath(id))));
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

#if defined(OS_WIN)
const SharedMemory::Handle kInvalidHandle = nullptr;
#else
const SharedMemory::Handle kInvalidHandle = -1;
#endif

SharedMemory::SharedMemory(int id,
                           base::win::ScopedHandle&& handle,
                           void* data,
                           std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
    : factory_proxy_(std::move(factory_proxy)),
      handle_(std::move(handle)),
      data_(data),
      id_(id)
{
    if (factory_proxy_)
        factory_proxy_->onSharedMemoryCreate(id_);
}

SharedMemory::~SharedMemory()
{
    if (factory_proxy_)
        factory_proxy_->onSharedMemoryDestroy(id_);

    UnmapViewOfFile(data_);
}

// static
std::unique_ptr<SharedMemory> SharedMemory::create(
    Mode mode, size_t size, std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
{
    static const int kRetryCount = 10;

    base::win::ScopedHandle file;
    int id;

    std::random_device device;
    std::mt19937 generator(device());

    std::uniform_int_distribution<> distance(0, std::numeric_limits<int>::max());

    for (int i = 0; i < kRetryCount; ++i)
    {
        id = distance(generator);

        if (createFileMapping(mode, id, size, &file))
            break;
    }

    if (!file.isValid())
        return nullptr;

    void* memory;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    memset(memory, 0, size);

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, std::move(file), memory, std::move(factory_proxy)));
}

// static
std::unique_ptr<SharedMemory> SharedMemory::open(
    Mode mode, int id, std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
{
    base::win::ScopedHandle file;
    if (!openFileMapping(mode, id, &file))
        return nullptr;

    void* memory;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, std::move(file), memory, std::move(factory_proxy)));
}

} // namespace ipc
