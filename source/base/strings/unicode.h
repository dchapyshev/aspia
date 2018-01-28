//
// PROJECT:         Aspia
// FILE:            base/strings/unicode.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__STRINGS__UNICODE_H
#define _ASPIA_BASE__STRINGS__UNICODE_H

#include <string>

namespace aspia {

bool UNICODEtoUTF8(const std::wstring& in, std::string& out);
bool UTF8toUNICODE(const std::string& in, std::wstring& out);

bool UNICODEtoUTF8(const wchar_t* in, std::string& out);
bool UTF8toUNICODE(const char* in, std::wstring& out);

std::wstring UNICODEfromUTF8(const std::string& in);
std::string UTF8fromUNICODE(const std::wstring& in);

std::wstring UNICODEfromUTF8(const char* in);
std::string UTF8fromUNICODE(const wchar_t* in);

} // namespace aspia

#endif // _ASPIA_BASE__STRINGS__UNICODE_H
