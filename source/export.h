//
// PROJECT:         Aspia
// FILE:            export.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA__EXPORT_H
#define _ASPIA__EXPORT_H

#if defined(CORE_IMPLEMENTATION)
#define CORE_EXPORT __declspec(dllexport)
#else
#define CORE_EXPORT __declspec(dllimport)
#endif // defined(CORE_IMPLEMENTATION)

#endif // _ASPIA__EXPORT_H
