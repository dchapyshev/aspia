/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/macros.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE_MACROS_H
#define _ASPIA_BASE_MACROS_H

// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#define DISALLOW_COPY_AND_ASSIGN(TypeName)   \
    TypeName(const TypeName&) = delete;      \
    void operator=(const TypeName&) = delete

#endif // _ASPIA_BASE_MACROS_H
