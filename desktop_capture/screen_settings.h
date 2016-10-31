/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/screen_settings.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SCREEN_SETTINGS_H
#define _ASPIA_SCREEN_SETTINGS_H

#include "util/thread.h"

class ScreenSettings : public Thread
{
public:
    ScreenSettings();
    ~ScreenSettings();

    bool IsChanged() const;

    static int Width()
    {
        return GetSystemMetrics(SM_CXSCREEN);
    }

    static int Height()
    {
        return GetSystemMetrics(SM_CYSCREEN);
    }

    static int ColorDepth()
    {
        DEVMODEW Mode = { 0 };

        Mode.dmSize = sizeof(DEVMODEW);
        if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &Mode))
        {
            Mode.dmBitsPerPel = 32;
        }

        if (Mode.dmBitsPerPel < 16)
            return 16;
        else if (Mode.dmBitsPerPel > 32)
            return 32;

        if (Mode.dmBitsPerPel != 16 &&
            Mode.dmBitsPerPel != 32)
            return 32;

        return Mode.dmBitsPerPel;
    }

    virtual void Processing() override;
    virtual void OnTerminate() override;

private:
    static LRESULT CALLBACK ScreenSettingDetectorWndProc(HWND hWindow, UINT Msg, WPARAM wParam, LPARAM lParam);

private:
    HWND hWindow_;

    bool is_changed_;

    int old_width_;
    int old_height_;
    int old_bits_per_pixel_;
};

#endif // _ASPIA_SCREEN_SETTINGS_H
