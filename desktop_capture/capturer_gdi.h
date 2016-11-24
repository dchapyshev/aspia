/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capturer_gdi.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H

#include "aspia_config.h"

#include "desktop_capture/capturer.h"
#include "desktop_capture/differ.h"
#include "desktop_capture/desktop_effects.h"
#include "desktop_capture/pixel_format.h"
#include "base/macros.h"
#include "base/scoped_thread_desktop.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"
#include "base/desktop_win.h"

//
// Класс захвата избражения экрана
// TODO: Захват изображения курсора
//
class CapturerGDI : public Capturer
{
public:
    CapturerGDI();
    ~CapturerGDI() {}

    //
    // Метод выполнения захвата экрана
    // Возвращает указатель на буфер, который содержит изображение экрана.
    //
    const uint8_t* CaptureImage(DesktopRegion &changed_region,
                                DesktopSize &desktop_size,
                                PixelFormat &pixel_format) override;

    static DesktopRect GetDesktopRect();

private:
    typedef struct
    {
        BITMAPINFOHEADER header;
        union
        {
            struct
            {
                uint32_t red;
                uint32_t green;
                uint32_t blue;
            } mask;
            RGBQUAD color[256];
        } u;
    } BitmapInfo;

    static void GetDefaultBitmapInfo(BitmapInfo *bmi);
    static PixelFormat GetPixelFormat(const BitmapInfo &bmi);
    void AllocateBuffer(int buffer_index, int align);
    void PrepareInputDesktop();
    void PrepareCaptureResources();

private:
    PixelFormat current_pixel_format_;

    std::unique_ptr<DesktopEffects> desktop_effects_;

    DesktopRect current_desktop_rect_;

    std::unique_ptr<Differ> differ_;
    ScopedThreadDesktop desktop_;

    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;

    static const int kNumBuffers = 2;
    int curr_buffer_id_;
    ScopedBitmap target_bitmap_[kNumBuffers];
    uint8_t *image_buffer_[kNumBuffers]; // Буфер изображения экрана

    DISALLOW_COPY_AND_ASSIGN(CapturerGDI);
};

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
