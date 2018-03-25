//
// PROJECT:         Aspia
// FILE:            base/strings/unicode.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

namespace {

bool WideToUtf8(std::wstring_view in, std::string& out)
{
    size_t in_len = in.length();
    if (!in_len)
        return false;

    int out_len = WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                                      nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                            &out[0], out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

bool Utf8ToWide(std::string_view in, std::wstring& out)
{
    const size_t in_len = in.length();
    if (!in_len)
        return false;

    const int out_len = MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                                            nullptr, 0);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                            &out[0], out_len) != out_len)
        return false;

    return true;
}

} // namespace

bool UNICODEtoUTF8(const std::wstring& in, std::string& out)
{
    return WideToUtf8(in, out);
}

bool UTF8toUNICODE(const std::string& in, std::wstring& out)
{
    return Utf8ToWide(in, out);
}

bool UNICODEtoUTF8(const wchar_t* in, std::string& out)
{
    if (!in)
        return false;

    return WideToUtf8(in, out);
}

bool UTF8toUNICODE(const char* in, std::wstring& out)
{
    if (!in)
        return false;

    return Utf8ToWide(in, out);
}

std::wstring UNICODEfromUTF8(const std::string& in)
{
    std::wstring out;
    UTF8toUNICODE(in, out);
    return out;
}

std::string UTF8fromUNICODE(const std::wstring& in)
{
    std::string out;
    UNICODEtoUTF8(in, out);
    return out;
}

std::wstring UNICODEfromUTF8(const char* in)
{
    std::wstring out;
    UTF8toUNICODE(in, out);
    return out;
}

std::string UTF8fromUNICODE(const wchar_t* in)
{
    std::string out;
    UNICODEtoUTF8(in, out);
    return out;
}

} // namespace aspia
