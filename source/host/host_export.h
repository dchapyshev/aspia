//
// PROJECT:         Aspia
// FILE:            host/host_export.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_EXPORT_H
#define _ASPIA_HOST__HOST_EXPORT_H

#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __declspec(dllexport)
#else
#define HOST_EXPORT __declspec(dllimport)
#endif // defined(HOST_IMPLEMENTATION)

#endif // _ASPIA_HOST__HOST_EXPORT_H
