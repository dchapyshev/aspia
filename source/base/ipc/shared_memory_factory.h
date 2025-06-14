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

#ifndef BASE_IPC_SHARED_MEMORY_FACTORY_H
#define BASE_IPC_SHARED_MEMORY_FACTORY_H

#include <QObject>

namespace base {

class SharedMemory;

class SharedMemoryFactory final : public QObject
{
    Q_OBJECT

public:
    explicit SharedMemoryFactory(QObject* parent = nullptr);
    ~SharedMemoryFactory();

    // Creates a new shared memory. If an error occurs, nullptr is returned.
    SharedMemory* create(size_t size);

    // Opens an existing shared memory.
    // If shared memory does not exist, nullptr is returned.
    // If any other error occurs, nullptr is returned.
    SharedMemory* open(int id);

signals:
    // Called when a shared memory is successfully created or opened.
    void sig_memoryCreated(int id);

    // Called when the shared memory is destroyed.
    void sig_memoryDestroyed(int id);

private:
    Q_DISABLE_COPY(SharedMemoryFactory)
};

} // namespace base

#endif // BASE_IPC_SHARED_MEMORY_FACTORY_H
