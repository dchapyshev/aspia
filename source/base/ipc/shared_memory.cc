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

#include <QRandomGenerator>

#include "base/logging.h"

#include <atomic>
#include <cstring>

#if defined(Q_OS_WINDOWS)
#include <AclAPI.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif // defined(Q_OS_UNIX)

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
int createUniqueId()
{
    static std::atomic_int last_id =
        QRandomGenerator::global()->bounded(0, std::numeric_limits<int>::max());
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
                                         qUtf16Printable(path)));
    if (!file.isValid())
    {
        PLOG(ERROR) << "CreateFileMappingW failed";
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LOG(ERROR) << "Already exists shared memory:" << path;
        return false;
    }

    DWORD error_code = SetSecurityInfo(
        file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, nullptr);
    if (error_code != ERROR_SUCCESS)
    {
        LOG(ERROR) << "SetSecurityInfo failed:" << SystemError::toString(error_code);
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

    ScopedHandle file(OpenFileMappingW(desired_access, FALSE, qUtf16Printable(createFilePath(id))));
    if (!file.isValid())
    {
        PLOG(ERROR) << "OpenFileMappingW failed";
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
        PLOG(ERROR) << "MapViewOfFile failed";
        return false;
    }

    return true;
}

#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
SharedMemory::SharedMemory(int id, ScopedPlatformHandle&& handle, void* data, QObject* parent)
    : QObject(parent),
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

#if defined(Q_OS_UNIX)
    struct stat info;
    if (fstat(handle_.get(), &info) != 0)
    {
        PLOG(ERROR) << "fstat failed";
    }
    else
    {
        munmap(data_, info.st_size);
    }

    QByteArray name = createFilePath(id_).toLocal8Bit();
    if (shm_unlink(name.data()) == -1)
    {
        PLOG(ERROR) << "shm_unlink failed";
    }
#endif // defined(Q_OS_UNIX)
}

//--------------------------------------------------------------------------------------------------
// static
SharedMemory* SharedMemory::create(Mode mode, size_t size)
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

    return new SharedMemory(id, std::move(file), memory);
#elif defined(Q_OS_UNIX)
    int id = -1;
    int fd = -1;

    for (int i = 0; i < kRetryCount; ++i)
    {
        id = createUniqueId();

        QByteArray name = createFilePath(id).toLocal8Bit();
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

        fd = shm_open(name.data(), open_flags, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            PLOG(ERROR) << "shm_open failed";
            continue;
        }

        break;
    }

    if (fd == -1)
    {
        LOG(ERROR) << "Unable to create shared memory";
        return nullptr;
    }

    if (ftruncate(fd, size) == -1)
    {
        PLOG(ERROR) << "ftruncate failed";
        return nullptr;
    }

    int protection = PROT_READ;
    if (mode == Mode::READ_WRITE)
        protection |= PROT_WRITE;

    void* memory = mmap(nullptr, size, protection, MAP_SHARED, fd, 0);
    if (!memory)
    {
        PLOG(ERROR) << "mmap failed";
        return nullptr;
    }

    memset(memory, 0, size);

    return new SharedMemory(id, ScopedPlatformHandle(fd), memory);
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
SharedMemory* SharedMemory::open(Mode mode, int id)
{
#if defined(Q_OS_WINDOWS)
    ScopedPlatformHandle file;
    if (!openFileMapping(mode, id, &file))
        return nullptr;

    void* memory = nullptr;
    if (!mapViewOfFile(mode, file, &memory))
        return nullptr;

    return new SharedMemory(id, std::move(file), memory);
#elif defined(Q_OS_UNIX)
    QByteArray name = createFilePath(id).toLocal8Bit();
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

    int fd = shm_open(name.data(), open_flags, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        PLOG(ERROR) << "shm_open failed";
        return nullptr;
    }

    struct stat info;
    if (fstat(fd, &info) != 0)
    {
        PLOG(ERROR) << "Unable to get shared memory size";
        return nullptr;
    }

    int protection = PROT_READ;
    if (mode == Mode::READ_WRITE)
        protection |= PROT_WRITE;

    void* memory = mmap(nullptr, info.st_size, protection, MAP_SHARED, fd, 0);
    if (!memory)
    {
        PLOG(ERROR) << "mmap failed";
        return nullptr;
    }

    return new SharedMemory(id, ScopedPlatformHandle(fd), memory);
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

} // namespace base
