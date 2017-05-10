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

bool ANSItoUNICODE(const std::string& in, std::wstring& out);
bool UNICODEtoANSI(const std::wstring& in, std::string& out);

bool UNICODEtoUTF8(const std::wstring& in, std::string& out);
bool UTF8toUNICODE(const std::string& in, std::wstring& out);

bool ANSItoUTF8(const std::string& in, std::string& out);
bool UTF8toANSI(const std::string& in, std::string& out);

std::wstring UNICODEfromANSI(const std::string& in);
std::string ANSIfromUNICODE(const std::wstring& in);

std::wstring UNICODEfromUTF8(const std::string& in);
std::string UTF8fromUNICODE(const std::wstring& in);

std::string UTF8fromANSI(const std::string& in);
std::string ANSIfromUTF8(const std::string& in);

} // namespace aspia

#endif // _ASPIA_BASE__UNICODE_H
