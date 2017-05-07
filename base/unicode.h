//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/unicode.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__UNICODE_H
#define _ASPIA_BASE__UNICODE_H

#include <string>

namespace aspia {

std::wstring UNICODEfromANSI(const std::string& in);
std::wstring UNICODEfromANSI(const char* in);

std::string ANSIfromUNICODE(const std::wstring& in);
std::string ANSIfromUNICODE(const WCHAR* in);

std::string UTF8fromUNICODE(const std::wstring& in);
std::string UTF8fromUNICODE(const WCHAR* in);

std::wstring UNICODEfromUTF8(const std::string& in);
std::wstring UNICODEfromUTF8(const char* in);

std::string ANSItoUTF8(const std::string& in);
std::string ANSItoUTF8(const char* in);

std::string UTF8toANSI(const std::string& in);
std::string UTF8toANSI(const char* in);

} // namespace aspia

#endif // _ASPIA_BASE__UNICODE_H
