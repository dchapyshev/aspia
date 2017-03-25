//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor_cache.h
// LICENSE:         See top-level directory
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
    MouseCursorCache(size_t cache_size);
    ~MouseCursorCache() = default;

    //
    // Ищет совпадающий курсор в кеше.
    // Если курсор уже находится в кеше, то возвращается индекс курсора в кеше.
    // Если курсора нет в кеше, то возвращается -1.
    //
    int Find(const MouseCursor* mouse_cursor);

    //
    // Добавленяет курсор в кеш и возвращает индекс добавленного элемента.
    //
    int Add(std::unique_ptr<MouseCursor> mouse_cursor);

    //
    // Возвращает указатель на закешированный курсор по его индексу в кеше.
    //
    MouseCursor* Get(int index);

    //
    // Проверяет наличие элементов в кеше.
    //
    bool IsEmpty() const;

    //
    // Очищает кеш.
    //
    void Clear();

    //
    // Текущий размер кеша.
    //
    int Size() const;

    static bool IsValidCacheSize(size_t size);

private:
    std::deque<std::unique_ptr<MouseCursor>> cache_;
    const size_t cache_size_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
