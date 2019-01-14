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

#ifndef ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
#define ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H

#include <deque>

#include "desktop_capture/mouse_cursor.h"

namespace desktop {

class MouseCursorCache
{
public:
    explicit MouseCursorCache(size_t cache_size);
    ~MouseCursorCache() = default;

    static const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

    // Looks for a matching cursor in the cache.
    // If the cursor is already in the cache, the cursor index in the cache is
    // returned.
    // If the cursor is not in the cache, -1 is returned.
    size_t find(const MouseCursor* mouse_cursor);

    // Adds the cursor to the cache and returns the index of the added element.
    size_t add(std::unique_ptr<MouseCursor> mouse_cursor);

    // Returns the pointer to the cached cursor by its index in the cache.
    std::shared_ptr<MouseCursor> get(size_t index);

    // Checks an empty cache or not.
    bool isEmpty() const;

    // Clears the cache.
    void clear();

    // The current size of the cache.
    size_t size() const { return cache_size_; }

    static bool isValidCacheSize(size_t size);

private:
    std::deque<std::shared_ptr<MouseCursor>> cache_;
    const size_t cache_size_;
};

} // namespace desktop

#endif // ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
