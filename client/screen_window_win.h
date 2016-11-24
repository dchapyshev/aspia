/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/screen_window_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__SCREEN_WINDOW_H
#define _ASPIA_CLIENT__SCREEN_WINDOW_H

#include "aspia_config.h"

#include <stdint.h>
#include <future>

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/pixel_format.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"
#include "base/thread.h"
#include "base/mutex.h"
#include "base/macros.h"
#include "proto/proto.pb.h"

class ScreenWindowWin : public Thread
{
public:
    typedef std::function<void()> OnClosedCallback;
    typedef std::function<void(std::unique_ptr<proto::ClientToServer>&)> OnMessageAvailableCallback;

    ScreenWindowWin(HWND parent, OnMessageAvailableCallback on_message, OnClosedCallback on_closed);
    ~ScreenWindowWin();

    class LockedMemory : public LockGuard<Mutex>
    {
    public:
        LockedMemory(uint8_t *buffer, Mutex *mutex) : buffer_(buffer), LockGuard<Mutex>(mutex) {}
        ~LockedMemory() {}

        uint8_t* get() const { return buffer_; }

    private:
        uint8_t *buffer_;
    };

    LockedMemory GetBuffer();
    void Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format);
    void Invalidate();

private:
    void Worker() override;
    void OnStop() override;

    void SendPointerEvent(int32_t x, int32_t y, int32_t mask);
    void SendKeyEvent(int32_t keycode, bool extended, bool pressed);
    void SendCAD(); // Sends CTRL+ALT+DEL

    static LRESULT CALLBACK WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnCreate(HWND window, LPARAM lParam);
    static void OnDestroy(HWND window);
    static void OnClose(HWND window);
    static void OnPaint(HWND window);
    static void OnHScroll(HWND window, WPARAM wParam);
    static void OnVScroll(HWND window, WPARAM wParam);
    static void OnSize(HWND window);
    static void OnMouseMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnKeyMessage(HWND window, WPARAM wParam, LPARAM lParam);

    static void GetWindowSize(HWND window, SIZE &size);
    static void Scroll(ScreenWindowWin *this_, POINT &delta, SIZE &window_size);
    static void UpdateScrollBars(ScreenWindowWin *this_, SIZE &window_size);
    static void FillNonScreenArea(ScreenWindowWin *this_, HDC hdc, RECT &paint_rect);

    void AllocateBuffer(int align);

private:
    OnMessageAvailableCallback on_message_;
    OnClosedCallback on_closed_;

    HWND parent_;
    HWND window_;

    DesktopSize screen_size_;
    PixelFormat pixel_format_;

    ScopedCreateDC screen_dc_;
    ScopedCreateDC memory_dc_;
    ScopedBitmap target_bitmap_;

    uint8_t *image_buffer_;
    Mutex image_buffer_lock_;

    POINT center_offset_;

    POINT scroll_pos_;
    POINT scroll_max_;

    DISALLOW_COPY_AND_ASSIGN(ScreenWindowWin);
};

#endif // _ASPIA_CLIENT__SCREEN_WINDOW_H
