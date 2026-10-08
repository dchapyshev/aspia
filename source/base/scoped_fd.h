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

#ifndef BASE_SCOPED_FD_H
#define BASE_SCOPED_FD_H

#include <QtClassHelperMacros>

#include <unistd.h>

// Owns a POSIX file descriptor and closes it on destruction.
class ScopedFd
{
public:
    ScopedFd() = default;

    explicit ScopedFd(int fd)
        : fd_(fd)
    {
        // Nothing
    }

    ~ScopedFd()
    {
        reset();
    }

    ScopedFd(ScopedFd&& other) noexcept
        : fd_(other.release())
    {
        // Nothing
    }

    ScopedFd& operator=(ScopedFd&& other) noexcept
    {
        reset(other.release());
        return *this;
    }

    int get() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }

    void reset(int fd = -1)
    {
        if (fd_ >= 0 && fd_ != fd)
            ::close(fd_);
        fd_ = fd;
    }

    [[nodiscard]] int release()
    {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

private:
    int fd_ = -1;

    Q_DISABLE_COPY(ScopedFd)
};

#endif // BASE_SCOPED_FD_H
