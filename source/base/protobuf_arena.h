//
// Aspia Project
// Copyright (C) 2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__PROTOBUF_ARENA_H
#define BASE__PROTOBUF_ARENA_H

#include <memory>

#include <google/protobuf/arena.h>

namespace base {

class WaitableTimer;
class TaskRunner;

class ProtobufArena
{
public:
    ~ProtobufArena();

    void setArenaStartSize(size_t size);
    void setArenaMaxSize(size_t size);

protected:
    static const size_t kArenaStartBlockSize = 16 * 1024; // 16 kB
    static const size_t kArenaMaxBlockSize = 32 * 1024; // 32 kB

    explicit ProtobufArena(std::shared_ptr<TaskRunner> task_runner);

    template<class T>
    T* messageFromArena()
    {
        return google::protobuf::Arena::CreateMessage<T>(&arena_);
    }

private:
    void reset();

    std::unique_ptr<WaitableTimer> cleanup_timer_;
    google::protobuf::Arena arena_;

    size_t arena_start_block_size_ = kArenaStartBlockSize;
    size_t arena_max_block_size_ = kArenaMaxBlockSize;
};

} // namespace base

#endif // BASE__PROTOBUF_ARENA_H
