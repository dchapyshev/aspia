//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/macros.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MACROS_H
#define _ASPIA_BASE__MACROS_H

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&) = delete

//
// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
//
#define DISALLOW_COPY_AND_ASSIGN(TypeName)   \
    TypeName(const TypeName&) = delete;      \
    void operator=(const TypeName&) = delete

//
// A macro to disallow all the implicit constructors, namely the default
// constructor, copy constructor and operator= functions.
//
// This should be used in the declarations for a class that wants to prevent
// anyone from instantiating it. This is especially useful for classes
// containing only static methods.
//
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#ifndef UNREF
#define UNREF(P)    (P)
#endif // UNREF

#endif // _ASPIA_BASE__MACROS_H
