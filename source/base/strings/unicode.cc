//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/unicode.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"

namespace aspia {

bool ANSItoUNICODE(const std::string& in, std::wstring& out)
{
    const int in_len = static_cast<int>(in.length());
    if (in_len <= 0)
        return false;

    const int out_len = MultiByteToWideChar(CP_ACP, 0, in.data(), in_len, nullptr, 0);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (MultiByteToWideChar(CP_ACP, 0, in.data(), in_len, &out[0], out_len) != out_len)
        return false;

    return true;
}

bool UNICODEtoANSI(const std::wstring& in, std::string& out)
{
    const int in_len = static_cast<int>(in.length());
    if (in_len <= 0)
        return false;

    const int out_len = WideCharToMultiByte(CP_ACP, 0, in.data(), in_len,
                                            nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (WideCharToMultiByte(CP_ACP, 0, in.data(), in_len,
                            &out[0], out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

bool ANSItoUNICODE(const char* in, std::wstring& out)
{
    if (!in || !in[0])
        return false;

    const size_t in_len = strlen(in);
    if (!in_len)
        return false;

    const int out_len = MultiByteToWideChar(CP_ACP, 0, in, in_len, nullptr, 0);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (MultiByteToWideChar(CP_ACP, 0, in, in_len, &out[0], out_len) != out_len)
        return false;

    return true;
}

bool UNICODEtoANSI(const wchar_t* in, std::string& out)
{
    if (!in || !in[0])
        return false;

    const size_t in_len = wcslen(in);
    if (!in_len)
        return false;

    const int out_len = WideCharToMultiByte(CP_ACP, 0, in, in_len, nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (WideCharToMultiByte(CP_ACP, 0, in, in_len, &out[0], out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

bool UNICODEtoUTF8(const std::wstring& in, std::string& out)
{
    const int in_len = static_cast<int>(in.length());
    if (in_len <= 0)
        return false;

    const int out_len = WideCharToMultiByte(CP_UTF8, 0, in.data(), in_len,
                                            nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (WideCharToMultiByte(CP_UTF8, 0, in.data(), in_len,
                            &out[0], out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

bool UTF8toUNICODE(const std::string& in, std::wstring& out)
{
    const int in_len = static_cast<int>(in.length());
    if (in_len <= 0)
        return false;

    const int out_len = MultiByteToWideChar(CP_UTF8, 0, in.data(), in_len, nullptr, 0);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (MultiByteToWideChar(CP_UTF8, 0, in.data(), in_len, &out[0], out_len) != out_len)
        return false;

    return true;
}

bool UNICODEtoUTF8(const wchar_t* in, std::string& out)
{
    if (!in || !in[0])
        return false;

    const size_t in_len = wcslen(in);
    if (!in_len)
        return false;

    const int out_len = WideCharToMultiByte(CP_UTF8, 0, in, in_len, nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (WideCharToMultiByte(CP_UTF8, 0, in, in_len, &out[0], out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

bool UTF8toUNICODE(const char* in, std::wstring& out)
{
    if (!in || !in[0])
        return false;

    const size_t in_len = strlen(in);
    if (!in_len)
        return false;

    const int out_len = MultiByteToWideChar(CP_UTF8, 0, in, in_len, nullptr, 0);
    if (out_len <= 0)
        return false;

    out.resize(out_len);

    if (MultiByteToWideChar(CP_UTF8, 0, in, in_len, &out[0], out_len) != out_len)
        return false;

    return true;
}

bool ANSItoUTF8(const std::string& in, std::string& out)
{
    std::wstring uni;

    if (!ANSItoUNICODE(in, uni))
        return false;

    return UNICODEtoUTF8(uni, out);
}

bool UTF8toANSI(const std::string& in, std::string& out)
{
    std::wstring uni;

    if (!UTF8toUNICODE(in, uni))
        return false;

    return UNICODEtoANSI(uni, out);
}

bool ANSItoUTF8(const char* in, std::string& out)
{
    std::wstring uni;

    if (!ANSItoUNICODE(in, uni))
        return false;

    return UNICODEtoUTF8(uni, out);
}

bool UTF8toANSI(const char* in, std::string& out)
{
    std::wstring uni;

    if (!UTF8toUNICODE(in, uni))
        return false;

    return UNICODEtoANSI(uni, out);
}

std::wstring UNICODEfromANSI(const std::string& in)
{
    std::wstring out;
    ANSItoUNICODE(in, out);
    return out;
}

std::string ANSIfromUNICODE(const std::wstring& in)
{
    std::string out;
    UNICODEtoANSI(in, out);
    return out;
}

std::wstring UNICODEfromANSI(const char* in)
{
    std::wstring out;
    ANSItoUNICODE(in, out);
    return out;
}

std::string ANSIfromUNICODE(const wchar_t* in)
{
    std::string out;
    UNICODEtoANSI(in, out);
    return out;
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

std::string UTF8fromANSI(const std::string& in)
{
    std::string out;
    ANSItoUTF8(in, out);
    return out;
}

std::string ANSIfromUTF8(const std::string& in)
{
    std::string out;
    UTF8toANSI(in, out);
    return out;
}

std::string UTF8fromANSI(const char* in)
{
    std::string out;
    ANSItoUTF8(in, out);
    return out;
}

std::string ANSIfromUTF8(const char* in)
{
    std::string out;
    UTF8toANSI(in, out);
    return out;
}

} // namespace aspia
