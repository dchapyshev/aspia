/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/screen_window.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__SCREEN_WINDOW_H
#define _ASPIA_CLIENT__SCREEN_WINDOW_H

#include "aspia_config.h"

#include <stdint.h>
#include <future>

#include "base/macros.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"
#include "base/thread.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/desktop_point.h"
#include "desktop_capture/pixel_format.h"
#include "client/client.h"
#include "proto/proto.pb.h"

namespace aspia {

class ScreenWindow : public Thread
{
public:
    ScreenWindow(HWND parent, std::unique_ptr<Socket> sock);
    ~ScreenWindow();

private:
    void Worker() override;
    void OnStop() override;

    void OnClientEvent(Client::Event type);
    void OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region);
    void OnVideoResize(const DesktopSize &size, const PixelFormat &format);

    void SendCAD(); // Sends CTRL+ALT+DEL

    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    LRESULT ProcessWindowMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate(HWND window, LPARAM lParam);
    void OnDestroy();
    void OnClose();
    void OnPaint();
    void OnHScroll(WPARAM wParam);
    void OnVScroll(WPARAM wParam);
    void OnSize();
    void OnMouseMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnKeyMessage(WPARAM wParam, LPARAM lParam);
    void OnActivate(WPARAM wParam);

    void Scroll(DesktopPoint &delta);
    void UpdateScrollBars();

    void DrawBackground(HDC hdc, const RECT &paint_rect);

    void AllocateBuffer(int align);

private:
    std::unique_ptr<Client> client_;

    HMODULE instance_;
    HWND parent_;
    HWND window_;

    ScopedHBRUSH background_brush_;

    DesktopSize screen_size_; // Рзамер удаленного экрана.
    PixelFormat pixel_format_;

    ScopedCreateDC screen_dc_;
    ScopedCreateDC memory_dc_;
    ScopedBitmap target_bitmap_;

    uint8_t *image_buffer_;
    Lock image_buffer_lock_;

    DesktopSize client_size_; // Размер клиентской области окна.
    DesktopPoint center_offset_;
    DesktopPoint scroll_pos_;

    HHOOK keyboard_hook_;

    DISALLOW_COPY_AND_ASSIGN(ScreenWindow);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__SCREEN_WINDOW_H
