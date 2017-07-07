//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor_cache.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
#define _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H

#include <deque>

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

class MouseCursorCache
{
public:
    explicit MouseCursorCache(size_t cache_size);
    ~MouseCursorCache() = default;

    static const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

    // Looks for a matching cursor in the cache.
    // If the cursor is already in the cache, the cursor index in the cache is returned.
    // If the cursor is not in the cache, -1 is returned.
    size_t Find(const MouseCursor* mouse_cursor);

    // Adds the cursor to the cache and returns the index of the added element.
    size_t Add(std::unique_ptr<MouseCursor> mouse_cursor);

    // Returns the pointer to the cached cursor by its index in the cache.
    std::shared_ptr<MouseCursor> Get(size_t index);

    // Checks an empty cache or not.
    bool IsEmpty() const;

    // Clears the cache.
    void Clear();

    // The current size of the cache.
    size_t Size() const;

    static bool IsValidCacheSize(size_t size);

private:
    std::deque<std::shared_ptr<MouseCursor>> cache_;
    const size_t cache_size_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
