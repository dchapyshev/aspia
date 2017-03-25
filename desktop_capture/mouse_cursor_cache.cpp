//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor_cache.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor_cache.h"

#include "base/logging.h"

namespace aspia {

static const size_t kMinCacheSize = 2;
static const size_t kMaxCacheSize = 31;

MouseCursorCache::MouseCursorCache(size_t cache_size) :
    cache_size_(cache_size)
{
    // Nothing
}

int MouseCursorCache::Find(const MouseCursor* mouse_cursor)
{
    DCHECK(mouse_cursor);

    int size = cache_.size();

    for (int index = 0; index < size; ++index)
    {
        // Если курсор найден к кеше.
        if (cache_.at(index)->IsEqual(*mouse_cursor))
        {
            // Возвращаем его индекс.
            return index;
        }
    }

    return -1;
}

int MouseCursorCache::Add(std::unique_ptr<MouseCursor> mouse_cursor)
{
    DCHECK(mouse_cursor);

    // Добавляем курсор в конец списка.
    cache_.push_back(std::move(mouse_cursor));

    // Если текущий размер кеша превышает максимальный размер кеша.
    if (cache_.size() > cache_size_)
    {
        // Удаляем первый элемент в кеше (самый старый).
        cache_.pop_front();
    }

    return cache_.size() - 1;
}

MouseCursor* MouseCursorCache::Get(int index)
{
    if (index < 0 || index > kMaxCacheSize)
    {
        DLOG(ERROR) << "Invalid cache index: " << index;
        return nullptr;
    }

    return cache_.at(index).get();
}

bool MouseCursorCache::IsEmpty() const
{
    return cache_.empty();
}

void MouseCursorCache::Clear()
{
    cache_.clear();
}

int MouseCursorCache::Size() const
{
    return cache_size_;
}

// static
bool MouseCursorCache::IsValidCacheSize(size_t size)
{
    if (size < kMinCacheSize || size > kMaxCacheSize)
    {
        return false;
    }

    return true;
}

} // namespace aspia
