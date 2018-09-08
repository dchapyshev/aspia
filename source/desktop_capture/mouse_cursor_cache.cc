//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop_capture/mouse_cursor_cache.h"

#include "base/logging.h"

namespace aspia {

namespace {

constexpr size_t kMinCacheSize = 2;
constexpr size_t kMaxCacheSize = 31;

} // namespace

MouseCursorCache::MouseCursorCache(size_t cache_size) :
    cache_size_(cache_size)
{
    // Nothing
}

size_t MouseCursorCache::find(const MouseCursor* mouse_cursor)
{
    DCHECK(mouse_cursor);

    size_t size = cache_.size();

    for (size_t index = 0; index < size; ++index)
    {
        // If the cursor is found in the cache.
        if (cache_.at(index)->isEqual(*mouse_cursor))
        {
            // Return its index.
            return index;
        }
    }

    return kInvalidIndex;
}

size_t MouseCursorCache::add(std::unique_ptr<MouseCursor> mouse_cursor)
{
    DCHECK(mouse_cursor);

    // Add the cursor to the end of the list.
    cache_.emplace_back(std::move(mouse_cursor));

    // If the current cache size exceeds the maximum cache size.
    if (cache_.size() > cache_size_)
    {
        // Delete the first element in the cache (the oldest one).
        cache_.pop_front();
    }

    return cache_.size() - 1;
}

std::shared_ptr<MouseCursor> MouseCursorCache::get(size_t index)
{
    if (index > kMaxCacheSize)
    {
        LOG(LS_WARNING) << "Invalid cache index: " << index;
        return nullptr;
    }

    return cache_.at(index);
}

bool MouseCursorCache::isEmpty() const
{
    return cache_.empty();
}

void MouseCursorCache::clear()
{
    cache_.clear();
}

// static
bool MouseCursorCache::isValidCacheSize(size_t size)
{
    if (size < kMinCacheSize || size > kMaxCacheSize)
        return false;

    return true;
}

} // namespace aspia
