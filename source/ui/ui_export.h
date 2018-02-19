//
// PROJECT:         Aspia
// FILE:            ui/ui_export.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__UI_EXPORT_H
#define _ASPIA_UI__UI_EXPORT_H

#if defined(UI_IMPLEMENTATION)
#define UI_EXPORT __declspec(dllexport)
#else
#define UI_EXPORT __declspec(dllimport)
#endif // defined(UI_IMPLEMENTATION)

#endif // _ASPIA_UI__UI_EXPORT_H
