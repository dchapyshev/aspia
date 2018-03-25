//
// PROJECT:         Aspia
// FILE:            desktop_capture/mouse_cursor.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
#define _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H

#include <QPoint>
#include <QSize>

#include <memory>

namespace aspia {

class MouseCursor
{
public:
    ~MouseCursor() = default;

    static std::unique_ptr<MouseCursor> create(std::unique_ptr<uint8_t[]> data,
                                               const QSize& size,
                                               const QPoint& hotspot);

    const QSize& size() const;
    const QPoint& hotSpot() const;
    uint8_t* data() const;

    int stride() const;

    bool isEqual(const MouseCursor& other);

private:
    MouseCursor(std::unique_ptr<uint8_t[]> data,
                const QSize& size,
                const QPoint& hotspot);

    std::unique_ptr<uint8_t[]> const data_;
    const QSize size_;
    const QPoint hotspot_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
