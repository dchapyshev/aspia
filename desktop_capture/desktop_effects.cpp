/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_effects.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/desktop_effects.h"

#include <versionhelpers.h>

#include "base/logging.h"

static const bool kForceEnable = 0;

typedef HRESULT(WINAPI *PDWMISCOMPOSITIONENABLED)(BOOL *pfEnabled);
typedef HRESULT(WINAPI *PDWMENABLECOMPOSITION)(UINT uCompositionAction);

static bool GetDragFullWindows()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS, 0, &value, 0);
    return value != FALSE;
}

static void SetDragFullWindows(bool value)
{
    SystemParametersInfoW(SPI_SETDRAGFULLWINDOWS, value, 0, SPIF_SENDCHANGE);
}

static bool GetDropShadow()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETDROPSHADOW, 0, &value, 0);
    return value != FALSE;
}

static void SetDropShadow(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETDROPSHADOW, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetAnimation()
{
    ANIMATIONINFO animation_info = { 0 };
    animation_info.cbSize = sizeof(animation_info);

    SystemParametersInfoW(SPI_GETANIMATION, sizeof(animation_info), &animation_info, 0);

    return (animation_info.iMinAnimate != 0);
}

static void SetAnimation(bool value)
{
    ANIMATIONINFO animation_info = { 0 };
    animation_info.cbSize = sizeof(animation_info);
    animation_info.iMinAnimate = value ? 1 : 0;

    SystemParametersInfoW(SPI_SETANIMATION, sizeof(animation_info), &animation_info, SPIF_SENDCHANGE);
}

static bool GetMenuAnimation()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETMENUANIMATION, 0, &value, 0);
    return value != FALSE;
}

static void SetMenuAnimation(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETMENUANIMATION, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetSelectionFade()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETSELECTIONFADE, 0, &value, 0);
    return value != FALSE;
}

static void SetSelectionFade(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETSELECTIONFADE, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetTooltipAnimation()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETTOOLTIPANIMATION, 0, &value, 0);
    return value != FALSE;
}

static void SetTooltipAnimation(bool value)
{
    ANIMATIONINFO animation_info = { 0 };
    animation_info.cbSize = sizeof(animation_info);
    animation_info.iMinAnimate = value ? 1 : 0;

    SystemParametersInfoW(SPI_SETTOOLTIPANIMATION, 0, &animation_info, SPIF_SENDCHANGE);
}

static bool GetCursorShadow()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETCURSORSHADOW, 0, &value, 0);
    return value != FALSE;
}

static void SetCursorShadow(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETCURSORSHADOW, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetComboboxAnimation()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETCOMBOBOXANIMATION, 0, &value, 0);
    return value != FALSE;
}

static void SetComboboxAnimation(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETCOMBOBOXANIMATION, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetUiEffects()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETUIEFFECTS, 0, &value, 0);
    return value != FALSE;
}

static void SetUiEffects(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETUIEFFECTS, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static std::wstring GetWallpaper()
{
    std::wstring wallpaper;

    wallpaper.resize(MAX_PATH);
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, wallpaper.size(), &wallpaper[0], 0);

    return wallpaper;
}

static void SetWallpaper(std::wstring &wallpaper)
{
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, &wallpaper[0], 0);
}

#ifndef SPI_GETCLIENTAREAANIMATION
#define SPI_GETCLIENTAREAANIMATION   0x1042
#endif // SPI_GETCLIENTAREAANIMATION

#ifndef SPI_SETCLIENTAREAANIMATION
#define SPI_SETCLIENTAREAANIMATION   0x1043
#endif // SPI_SETCLIENTAREAANIMATION

static bool GetClientAreaAnimation()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &value, 0);
    return value != FALSE;
}

static void SetClientAreaAnimation(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetGradientCaptions()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETGRADIENTCAPTIONS, 0, &value, 0);
    return value != FALSE;
}

static void SetGradientCaptions(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETGRADIENTCAPTIONS, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

static bool GetListBoxSmoothScrolling()
{
    BOOL value = FALSE;
    SystemParametersInfoW(SPI_GETLISTBOXSMOOTHSCROLLING, 0, &value, 0);
    return value != FALSE;
}

static void SetListBoxSmoothScrolling(bool value)
{
    BOOL v = value;
    SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING, 0, reinterpret_cast<PVOID>(v), SPIF_SENDCHANGE);
}

DesktopEffects::DesktopEffects()
{
    // “ень окон
    drop_shadow_ = GetDropShadow();
    if (drop_shadow_) SetDropShadow(false);

    // ѕеремещение всего окна
    drag_full_windows_ = GetDragFullWindows();
    if (drag_full_windows_) SetDragFullWindows(false);

    // јнимаци¤ сворачивани¤-разворачивани¤ окон
    animation_ = GetAnimation();
    if (animation_) SetAnimation(false);

    // јнимаци¤ меню
    menu_animation_ = GetMenuAnimation();
    if (menu_animation_) SetMenuAnimation(false);

    // ѕлавное затухание меню после нажати¤
    selection_fade_ = GetSelectionFade();
    if (selection_fade_) SetSelectionFade(false);

    // јнимаци¤ подсказок
    tooltip_animation_ = GetTooltipAnimation();
    if (tooltip_animation_) SetTooltipAnimation(false);

    // “ень под указателем мыши
    cursor_shadow_ = GetCursorShadow();
    if (cursor_shadow_) SetCursorShadow(false);

    // ѕлавна¤ прокрутка списков
    combobox_animation_ = GetComboboxAnimation();
    if (combobox_animation_) SetComboboxAnimation(false);

    // Ёффекты пользовательского интерфейса
    ui_effects_ = GetUiEffects();
    if (ui_effects_) SetUiEffects(false);

    // √радиент заголовков окон
    gradient_captions_ = GetGradientCaptions();
    if (gradient_captions_) SetGradientCaptions(false);

    // ѕлавное прокручивание
    listbox_smooth_scrolling_ = GetListBoxSmoothScrolling();
    if (listbox_smooth_scrolling_) SetListBoxSmoothScrolling(false);

    // ќбои рабочего стола
    wallpaper_ = GetWallpaper();
    if (wallpaper_[0]) SetWallpaper(std::wstring(L""));

    if (IsWindowsVistaOrGreater())
    {
        dwapi_.reset(new ScopedNativeLibrary("dwmapi.dll"));

        //
        // јктуально только дл¤ Windows Vista и Windows 7 (XP/2003 не поддерживает композицию,
        // начина¤ с Windows 8 композицию нельз¤ отключить программно)
        //
        composition_enabled_ = IsCompositionEnabled();
        if (composition_enabled_)
        {
            LOG(INFO) << "composition enabled. disable it...";
            EnableComposition(false);
        }

        // јнимаци¤ внутри окна
        client_area_animation_ = GetClientAreaAnimation();
        if (client_area_animation_) SetClientAreaAnimation(false);
    }
}

DesktopEffects::~DesktopEffects()
{
    // ѕеремещение всего окна
    if (kForceEnable || drag_full_windows_ != GetDragFullWindows())
        SetDragFullWindows(true);

    // јнимаци¤ сворачивани¤-разворачивани¤ окон
    if (kForceEnable || animation_ != GetAnimation())
        SetAnimation(true);

    // јнимаци¤ меню
    if (kForceEnable || menu_animation_ != GetMenuAnimation())
        SetMenuAnimation(true);

    // ѕлавное затухание меню после нажати¤
    if (kForceEnable || selection_fade_ != GetSelectionFade())
        SetSelectionFade(true);

    // јнимаци¤ подсказок
    if (kForceEnable || tooltip_animation_ != GetTooltipAnimation())
        SetTooltipAnimation(true);

    // “ень под указателем мыши
    if (kForceEnable || cursor_shadow_ != GetCursorShadow())
        SetCursorShadow(true);

    // ѕлавна¤ прокрутка списков
    if (kForceEnable || combobox_animation_ != GetComboboxAnimation())
        SetComboboxAnimation(true);

    // Ёффекты пользовательского интерфейса
    if (kForceEnable || ui_effects_ != GetUiEffects())
        SetUiEffects(true);

    // √радиент заголовков окон
    if (kForceEnable || gradient_captions_ != GetGradientCaptions())
        SetGradientCaptions(true);

    // ѕлавное прокручивание
    if (kForceEnable || listbox_smooth_scrolling_ != GetListBoxSmoothScrolling())
        SetListBoxSmoothScrolling(true);

    // ќбои рабочего стола
    if (kForceEnable || wallpaper_ != GetWallpaper())
        SetWallpaper(wallpaper_);

    if (IsWindowsVistaOrGreater())
    {
        if (kForceEnable || composition_enabled_)
        {
            LOG(INFO) << "enable composition...";
            EnableComposition(true);
        }

        // јнимаци¤ внутри окна
        if (kForceEnable || client_area_animation_ != GetClientAreaAnimation())
            SetClientAreaAnimation(true);
    }

    // “ень окон
    if (kForceEnable || drop_shadow_ != GetDropShadow())
        SetDropShadow(true);
}

bool DesktopEffects::IsCompositionEnabled()
{
    PDWMISCOMPOSITIONENABLED is_enabled =
        reinterpret_cast<PDWMISCOMPOSITIONENABLED>(dwapi_->GetFunctionPointer("DwmIsCompositionEnabled"));

    if (!is_enabled)
        return false;

    BOOL value = FALSE;
    is_enabled(&value);

    return (value != FALSE);
}

void DesktopEffects::EnableComposition(bool enable)
{
    PDWMENABLECOMPOSITION enable_composition =
        reinterpret_cast<PDWMENABLECOMPOSITION>(dwapi_->GetFunctionPointer("DwmEnableComposition"));

    if (!enable_composition)
        return;

    enable_composition(enable ? 1 : 0);
}
