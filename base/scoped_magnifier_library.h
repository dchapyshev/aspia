/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_magnifier_library.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_MAGNIFIER_LIBRARY_H
#define _ASPIA_BASE__SCOPED_MAGNIFIER_LIBRARY_H

#include "aspia_config.h"

#include <magnification.h>

#include "base/scoped_native_library.h"
#include "base/exception.h"

class ScopedMagnifierLibrary : private ScopedNativeLibrary
{
public:
    ScopedMagnifierLibrary() :
        ScopedNativeLibrary("magnification.dll")
    {
        // Initialize Magnification API function pointers.
        mag_initialize_func_ = reinterpret_cast<MagInitializeFunc>(
            GetFunctionPointer("MagInitialize"));
        mag_uninitialize_func_ = reinterpret_cast<MagUninitializeFunc>(
            GetFunctionPointer("MagUninitialize"));
        set_window_source_func_ = reinterpret_cast<MagSetWindowSourceFunc>(
            GetFunctionPointer("MagSetWindowSource"));
        set_window_filter_list_func_ = reinterpret_cast<MagSetWindowFilterListFunc>(
            GetFunctionPointer("MagSetWindowFilterList"));
        set_image_scaling_callback_func_ =
            reinterpret_cast<MagSetImageScalingCallbackFunc>(
            GetFunctionPointer("MagSetImageScalingCallback"));

        if (!mag_initialize_func_ || !mag_uninitialize_func_ ||
            !set_window_source_func_ || !set_window_filter_list_func_ ||
            !set_image_scaling_callback_func_)
        {
            throw Exception("Unable to initialize magnification library.");
        }

        if (!mag_initialize_func_())
        {
            throw Exception("Unable to initialize magnifier.");
        }
    }

    ~ScopedMagnifierLibrary()
    {
        mag_uninitialize_func_();
    }

    BOOL MagSetWindowSource(HWND hwnd, RECT rect)
    {
        return set_window_source_func_(hwnd, rect);
    }

    BOOL MagSetWindowFilterList(HWND hwnd,
                                DWORD dwFilterMode,
                                int count,
                                HWND* pHWND)
    {
        return set_window_filter_list_func_(hwnd, dwFilterMode, count, pHWND);
    }

    BOOL MagSetImageScalingCallback(HWND hwnd,
                                    MagImageScalingCallback callback)
    {
        return set_image_scaling_callback_func_(hwnd, callback);
    }

private:
    typedef BOOL(WINAPI* MagImageScalingCallback)(HWND hwnd,
                                                  void* srcdata,
                                                  MAGIMAGEHEADER srcheader,
                                                  void* destdata,
                                                  MAGIMAGEHEADER destheader,
                                                  RECT unclipped,
                                                  RECT clipped,
                                                  HRGN dirty);
    typedef BOOL(WINAPI* MagInitializeFunc)(void);
    typedef BOOL(WINAPI* MagUninitializeFunc)(void);
    typedef BOOL(WINAPI* MagSetWindowSourceFunc)(HWND hwnd, RECT rect);
    typedef BOOL(WINAPI* MagSetWindowFilterListFunc)(HWND hwnd,
                                                     DWORD dwFilterMode,
                                                     int count,
                                                     HWND* pHWND);
    typedef BOOL(WINAPI* MagSetImageScalingCallbackFunc)(HWND hwnd,
                                                         MagImageScalingCallback callback);

    MagInitializeFunc mag_initialize_func_ = nullptr;
    MagUninitializeFunc mag_uninitialize_func_ = nullptr;
    MagSetWindowSourceFunc set_window_source_func_ = nullptr;
    MagSetWindowFilterListFunc set_window_filter_list_func_ = nullptr;
    MagSetImageScalingCallbackFunc set_image_scaling_callback_func_ = nullptr;
};

#endif // _ASPIA_BASE__SCOPED_MAGNIFIER_LIBRARY_H
