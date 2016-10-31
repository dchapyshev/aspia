/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/screen_settings.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "screen_settings.h"

#define WM_SETTINGS_UPDATED (WM_APP + 1)

ScreenSettings::ScreenSettings() :
    is_changed_(true),
    hWindow_(nullptr)
{
    old_bits_per_pixel_ = ColorDepth();
    old_width_ = Width();
    old_height_ = Height();
}

ScreenSettings::~ScreenSettings()
{
    // Ничего не делаем
}

bool ScreenSettings::IsChanged() const
{
    bool value = is_changed_;

    if (value)
    {
        SendMessageW(hWindow_, WM_SETTINGS_UPDATED, 0, 0);
    }

    return value;
}

// Static
LRESULT CALLBACK ScreenSettings::ScreenSettingDetectorWndProc(HWND hWindow, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
        case WM_CREATE:
        {
            ScreenSettings *this_ =
                reinterpret_cast<ScreenSettings*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

            SetWindowLongPtrW(hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this_));
        }
        break;

        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
        {
            ScreenSettings *this_ =
                reinterpret_cast<ScreenSettings*>(GetWindowLongPtrW(hWindow, GWLP_USERDATA));

            if (this_)
            {
                if (this_->old_bits_per_pixel_ != this_->ColorDepth() ||
                    this_->old_width_  != this_->Width() ||
                    this_->old_height_ != this_->Height())
                {
                    this_->is_changed_ = true;
                }
            }
        }
        break;

        case WM_SETTINGS_UPDATED:
        {
            ScreenSettings *this_ =
                reinterpret_cast<ScreenSettings*>(GetWindowLongPtrW(hWindow, GWLP_USERDATA));

            if (this_) this_->is_changed_ = false;
        }
        break;

        case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
        break;
    }

    return DefWindowProcW(hWindow, Msg, wParam, lParam);
}

void ScreenSettings::Processing()
{
    const WCHAR class_name[] = L"AspiaScreenSettingsDetector";

    WNDCLASSEXW wnd_class = { 0 };

    wnd_class.cbSize        = sizeof(wnd_class);
    wnd_class.lpfnWndProc   = reinterpret_cast<WNDPROC>(ScreenSettingDetectorWndProc);
    wnd_class.lpszClassName = class_name;

    RegisterClassExW(&wnd_class);

    hWindow_ = CreateWindowExW(0,
                               class_name,
                               0,
                               WS_OVERLAPPEDWINDOW,
                               0, 0, 0, 0,
                               HWND_DESKTOP,
                               nullptr,
                               nullptr,
                               this);
    if (!hWindow_)
    {
        LOG(FATAL) << "CreateWindowExW() failed: " << GetLastError();
        return;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(class_name, nullptr);
}

void ScreenSettings::OnTerminate()
{
    DestroyWindow(hWindow_);
}
