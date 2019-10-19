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

#ifndef IPC__SHARED_MEMORY_H
#define IPC__SHARED_MEMORY_H

#include "base/win/scoped_object.h"

#include <memory>
#include <string>

namespace ipc {

class SharedMemory
{
public:
    ~SharedMemory();

    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

    static std::unique_ptr<SharedMemory> create(Mode mode, std::string_view name, size_t size);
    static std::unique_ptr<SharedMemory> map(Mode mode, std::string_view name);
    static std::string createUniqueName();

    void* get() { return memory_; }

private:
    SharedMemory(base::win::ScopedHandle&& file, void* memory);

    base::win::ScopedHandle file_;
    void* memory_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

} // namespace ipc

#endif // IPC__SHARED_MEMORY_H
