/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capturer.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_H

#include "desktop_capture/desktop_region_win.h"
#include "desktop_capture/pixel_format.h"

class Capturer
{
public:
    Capturer() {}
    virtual ~Capturer() {}

    //
    // Метод выполнения захвата экрана
    // Возвращает указатель на буфер, который содержит изображение экрана.
    //
    virtual const uint8_t* CaptureImage(DesktopRegion &changed_region,
                                        DesktopSize &desktop_size,
                                        PixelFormat &pixel_format) = 0;
};

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
