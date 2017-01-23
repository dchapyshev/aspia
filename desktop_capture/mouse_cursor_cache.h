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
    MouseCursorCache(size_t cache_size = 12);
    ~MouseCursorCache();

    //
    // »щет совпадающий курсор в кеше.
    // ≈сли курсор уже находитс€ в кеше, то возвращаетс€ индекс курсора в кеше.
    // ≈сли курсора нет в кеше, то возвращаетс€ -1.
    //
    int Find(const MouseCursor *mouse_cursor);

    //
    // ƒобавлен€ет курсор в кеш и возвращает индекс добавленного элемента.
    //
    int Add(std::unique_ptr<MouseCursor> mouse_cursor);

    //
    // ¬озвращает указатель на закешированный курсор по его индексу в кеше.
    //
    MouseCursor* Get(int index);

    //
    // ѕровер€ет наличие элементов в кеше.
    //
    bool IsEmpty() const;

    //
    // ќчищает кеш.
    //
    void Clear();

private:
    std::deque<std::unique_ptr<MouseCursor>> cache_;
    const size_t cache_size_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
