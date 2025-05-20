//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

#include <atomic>
#include <cstring>
#include <random>

#if defined(Q_OS_WINDOWS)
#include <AclAPI.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif // defined(Q_OS_LINUX)

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
int randomInt()
{
    std::random_device device;
    std::mt19937 engine(device());

    std::uniform_int_distribution<> distance(0, std::numeric_limits<int>::max());
    return distance(engine);
}

//--------------------------------------------------------------------------------------------------
int createUniqueId()
{
    static std::atomic_int last_id = randomInt();
    return last_id++;
}

//--------------------------------------------------------------------------------------------------
QString createFilePath(int id)
{
#if defined(Q_OS_WINDOWS)
    static const char kPrefix[] = "Global\\aspia_";
#else
    static const char kPrefix[] = "/aspia_shm_";
#endif
    return kPrefix + QString::number(id);
}

#if defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
bool createFileMapping(SharedMemory::Mode mode, int id, size_t size, ScopedHandle* out)
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

    QString path = createFilePath(id);

    ScopedHandle file(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, protect, high, low,
                                         reinterpret_cast<const wchar_t*>(path.utf16())));
    if (!file.isValid())
    {
        PLOG(LS_ERROR) << "CreateFileMappingW failed";
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LOG(LS_ERROR) << "Already exists shared memory: " << path;
        return false;
    }

    DWORD error_code = SetSecurityInfo(
        file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, nullptr);
    if (error_code != ERROR_SUCCESS)
    {
        LOG(LS_ERROR) << "SetSecurityInfo failed: " << SystemError::toString(error_code);
        return false;
    }

    out->swap(file);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool openFileMapping(SharedMemory::Mode mode, int id, ScopedHandle* out)
{
    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    ScopedHandle file(OpenFileMappingW(desired_access, FALSE,
        reinterpret_cast<const wchar_t*>(createFilePath(id).utf16())));
    if (!file.isValid())
    {
        PLOG(LS_ERROR) << "OpenFileMappingW failed";
        return false;
    }

    out->swap(file);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool mapViewOfFile(SharedMemory::Mode mode, HANDLE file, void** memory)
{
    DWORD desired_access;
    if (!modeToDesiredAccess(mode, &desired_access))
        return false;

    *memory = MapViewOfFile(file, desired_access, 0, 0, 0);
    if (!*memory)
    {
        PLOG(LS_ERROR) << "MapViewOfFile failed";
        return false;
    }

    return true;
}

#endif // defined(Q_OS_WINDOWS)

} // namespace

#if defined(Q_OS_WINDOWS)
const SharedMemory::PlatformHandle kInvalidHandle = nullptr;
#else
const SharedMemory::PlatformHandle kInvalidHandle = -1;
#endif

//--------------------------------------------------------------------------------------------------
SharedMemory::SharedMemory(int id,
                           ScopedPlatformHandle&& handle,
                           void* data,
                           QObject* parent)
    : SharedMemoryBase(parent),
      handle_(std::move(handle)),
      data_(data),
      id_(id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedMemory::~SharedMemory()
{
    emit sig_destroyed(id_);

#if defined(Q_OS_WINDOWS)
    UnmapViewOfFile(data_);
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
    struct stat info;
    if (fstat(handle_.get(), &info) != 0)
    {
        PLOG(LS_ERROR) << "fstat failed";
    }
    else
    {
        munmap(data_, info.st_size);
    }

    std::string name = base::local8BitFromUtf16(createFilePath(id_));
    if (shm_unlink(name.c_str()) == -1)
    {
        PLOG(LS_ERROR) << "shm_unlink failed";
    }
#endif // defined(Q_OS_LINUX)
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<SharedMemory> SharedMemory::create(Mode mode, size_t size)
{
    static const int kRetryCount = 10;

#if defined(Q_OS_WINDOWS)
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

    return std::unique_ptr<SharedMemory>(new SharedMemory(id, std::move(file), memory));
#elif defined(Q_OS_LINUX)
    int id = -1;
    int fd = -1;

    for (int i = 0; i < kRetryCount; ++i)
    {
        id = createUniqueId();

        std::string name = base::local8BitFromUtf16(createFilePath(id));
        int open_flags = O_CREAT;

        if (mode == Mode::READ_ONLY)
        {
            open_flags |= O_RDONLY;
        }
        else
        {
            DCHECK_EQ(mode, Mode::READ_WRITE);
            open_flags |= O_RDWR;
        }

        fd = shm_open(name.c_str(), open_flags, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            PLOG(LS_ERROR) << "shm_open failed";
            continue;
        }

        break;
    }

    if (fd == -1)
    {
        LOG(LS_ERROR) << "Unable to create shared memory";
        return nullptr;
    }

    if (ftruncate(fd, size) == -1)
    {
        PLOG(LS_ERROR) << "ftruncate failed";
        return nullptr;
    }

    int protection = PROT_READ;
    if (mode == Mode::READ_WRITE)
        protection |= PROT_WRITE;

    void* memory = mmap(nullptr, size, protection, MAP_SHARED, fd, 0);
    if (!memory)
    {
        PLOG(LS_ERROR) << "mmap failed";
        return nullptr;
    }

    memset(memory, 0, size);

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, ScopedPlatformHandle(fd), memory, std::move(factory_proxy)));
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<SharedMemory> SharedMemory::open(Mode mode, int id)
{
#if defined(Q_OS_WINDOWS)
    ScopedPlatformHandle file;
    if (!openFileMapping(mode, id, &file))
        return nullptr;

    void* memory = nullptr;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    return std::unique_ptr<SharedMemory>(new SharedMemory(id, std::move(file), memory));
#elif defined(Q_OS_LINUX)
    std::string name = base::local8BitFromUtf16(createFilePath(id));
    int open_flags = 0;

    if (mode == Mode::READ_ONLY)
    {
        open_flags |= O_RDONLY;
    }
    else
    {
        DCHECK_EQ(mode, Mode::READ_WRITE);
        open_flags |= O_RDWR;
    }

    int fd = shm_open(name.c_str(), open_flags, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        PLOG(LS_ERROR) << "shm_open failed";
        return nullptr;
    }

    struct stat info;
    if (fstat(fd, &info) != 0)
    {
        PLOG(LS_ERROR) << "Unable to get shared memory size";
        return nullptr;
    }

    int protection = PROT_READ;
    if (mode == Mode::READ_WRITE)
        protection |= PROT_WRITE;

    void* memory = mmap(nullptr, info.st_size, protection, MAP_SHARED, fd, 0);
    if (!memory)
    {
        PLOG(LS_ERROR) << "mmap failed";
        return nullptr;
    }

    return std::unique_ptr<SharedMemory>(
        new SharedMemory(id, ScopedPlatformHandle(fd), memory, std::move(factory_proxy)));
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

} // namespace base
