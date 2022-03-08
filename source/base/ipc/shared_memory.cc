//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/ipc/shared_memory.h"

#include "base/ipc/shared_memory_factory_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

#if defined(USE_PCG_GENERATOR)
#include "third_party/pcg-cpp/pcg_random.hpp"
#endif // defined(USE_PCG_GENERATOR)

#include <atomic>
#include <random>

#if defined(OS_WIN)
#include <AclAPI.h>
#endif // defined(OS_WIN)

namespace base {

namespace {

int randomInt()
{
#if defined(USE_PCG_GENERATOR)
    pcg_extras::seed_seq_from<std::random_device> device;
    pcg32_fast engine(device);
#else // defined(USE_PCG_GENERATOR)
    std::random_device device;
    std::mt19937 engine(device());
#endif

    std::uniform_int_distribution<> distance(0, std::numeric_limits<int>::max());
    return distance(engine);
}

int createUniqueId()
{
    static std::atomic_int last_id = randomInt();
    return last_id++;
}

std::u16string createFilePath(int id)
{
    static const char16_t kPrefix[] = u"Global\\aspia_";
    return kPrefix + numberToString16(id);
}

#if defined(OS_WIN)

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

bool createFileMapping(SharedMemory::Mode mode, int id, size_t size, win::ScopedHandle* out)
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

    win::ScopedHandle file(CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, protect, high, low, asWide(path)));
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
        LOG(LS_WARNING) << "SetSecurityInfo failed: " << SystemError::toString(error_code);
        return false;
    }

    out->swap(file);
    return true;
}

bool openFileMapping(SharedMemory::Mode mode, int id, win::ScopedHandle* out)
{
    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    win::ScopedHandle file(OpenFileMappingW(desired_access, FALSE, asWide(createFilePath(id))));
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

#endif // defined(OS_WIN)

} // namespace

#if defined(OS_WIN)
const SharedMemory::PlatformHandle kInvalidHandle = nullptr;
#else
const SharedMemory::PlatformHandle kInvalidHandle = -1;
#endif

SharedMemory::SharedMemory(int id,
                           ScopedPlatformHandle&& handle,
                           void* data,
                           base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
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

#if defined(OS_WIN)
    UnmapViewOfFile(data_);
#endif // defined(OS_WIN)
}

// static
std::unique_ptr<SharedMemory> SharedMemory::create(
    Mode mode, size_t size, base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
{
#if defined(OS_WIN)
    static const int kRetryCount = 10;

    ScopedPlatformHandle file;
    int id = -1;

    for (int i = 0; i < kRetryCount; ++i)
    {
        id = createUniqueId();
        if (createFileMapping(mode, id, size, &file))
            break;
    }

    if (!file.isValid())
        return nullptr;

    void* memory = nullptr;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    memset(memory, 0, size);

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, std::move(file), memory, std::move(factory_proxy)));
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

// static
std::unique_ptr<SharedMemory> SharedMemory::open(
    Mode mode, int id, base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy)
{
#if defined(OS_WIN)
    ScopedPlatformHandle file;
    if (!openFileMapping(mode, id, &file))
        return nullptr;

    void* memory = nullptr;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, std::move(file), memory, std::move(factory_proxy)));
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

} // namespace base
