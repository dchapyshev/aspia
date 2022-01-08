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

#include "base/protobuf_arena_helper.h"

#include "base/logging.h"

namespace base {

namespace {

void* tbbBlockAlloc(size_t size)
{
    return malloc(size);
}

void tbbBlockDealloc(void* ptr, size_t /* unused */)
{
    free(ptr);
}

} // namespace

ProtobufArenaHelper::ProtobufArenaHelper()
{
    arena_buffer_ = reinterpret_cast<char*>(tbbBlockAlloc(kArenaBufferSize));
    CHECK(arena_buffer_);

    resetArena();
}

ProtobufArenaHelper::~ProtobufArenaHelper()
{
    tbbBlockDealloc(arena_buffer_, 0);
}

void ProtobufArenaHelper::resetArena()
{
    google::protobuf::ArenaOptions options;
    options.block_alloc        = tbbBlockAlloc;
    options.block_dealloc      = tbbBlockDealloc;
    options.start_block_size   = kArenaStartBlockSize;
    options.max_block_size     = kArenaMaxBlockSize;
    options.initial_block      = arena_buffer_;
    options.initial_block_size = kArenaStartBlockSize;

    arena_.Reset();
    arena_.Init(options);
}

} // namespace base
