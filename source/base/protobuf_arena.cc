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

#include "base/protobuf_arena.h"

#include "base/logging.h"
#include "base/waitable_timer.h"

namespace base {

ProtobufArena::ProtobufArena(std::shared_ptr<TaskRunner> task_runner)
    : cleanup_timer_(std::make_unique<WaitableTimer>(
          WaitableTimer::Type::REPEATED, std::move(task_runner)))
{
    LOG(LS_INFO) << "Ctor";

    cleanup_timer_->start(std::chrono::seconds(60), [this]()
    {
        if (arena_.SpaceUsed() >= arena_start_block_size_)
            reset();
    });

    reset();
}

ProtobufArena::~ProtobufArena()
{
    LOG(LS_INFO) << "Dtor";
}

void ProtobufArena::setArenaStartSize(size_t size)
{
    arena_start_block_size_ = size;
}

void ProtobufArena::setArenaMaxSize(size_t size)
{
    arena_max_block_size_ = size;
}

void ProtobufArena::reset()
{
    LOG(LS_INFO) << "Reset arena (start size: " << arena_start_block_size_
                 << " max size: " << arena_max_block_size_ << ")";

    google::protobuf::ArenaOptions options;
    options.start_block_size = arena_start_block_size_;
    options.max_block_size = arena_max_block_size_;

    arena_.Reset();
    arena_.Init(options);
}

} // namespace base
