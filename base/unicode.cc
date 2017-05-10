//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/unicode.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/unicode.h"

namespace aspia {

bool ANSItoUNICODE(const std::string& in, std::wstring& out)
{
    int len = MultiByteToWideChar(CP_ACP, 0, in.data(), in.length(), nullptr, 0);
    if (len > 0)
    {
        out.resize(len);
        if (MultiByteToWideChar(CP_ACP, 0, in.data(), in.length(), &out[0], len) == len)
        {
            return true;
        }
    }

    return false;
}

bool UNICODEtoANSI(const std::wstring& in, std::string& out)
{
    int len = WideCharToMultiByte(CP_ACP, 0, in.data(), in.length(), nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        out.resize(len);
        if (WideCharToMultiByte(CP_ACP, 0, in.data(), in.length(), &out[0], out.length(), nullptr, nullptr) == len)
        {
            return true;
        }
    }

    return false;
}

bool UNICODEtoUTF8(const std::wstring& in, std::string& out)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, in.data(), in.length(), nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        out.resize(len);
        if (WideCharToMultiByte(CP_UTF8, 0, in.data(), in.length(), &out[0], len, nullptr, nullptr) == len)
        {
            return true;
        }
    }

    return false;
}

bool UTF8toUNICODE(const std::string& in, std::wstring& out)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, in.data(), in.length(), nullptr, 0);
    if (len > 0)
    {
        out.resize(len);
        if (MultiByteToWideChar(CP_UTF8, 0, in.data(), in.length(), &out[0], len) == len)
        {
            return true;
        }
    }

    return false;
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

} // namespace aspia
