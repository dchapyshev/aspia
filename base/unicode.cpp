//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/unicode.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/unicode.h"

namespace aspia {

std::wstring UNICODEfromANSI(const std::string& in)
{
    int len = ::MultiByteToWideChar(CP_ACP, 0, in.data(), in.length(), nullptr, 0);
    if (len > 0)
    {
        std::wstring uni;
        uni.resize(len);
        if (::MultiByteToWideChar(CP_ACP, 0, in.data(), in.length(), &uni[0], len) != 0)
        {
            return uni;
        }
    }

    return std::wstring();
}

std::wstring UNICODEfromANSI(const char* in)
{
    if (in && in[0] != 0)
    {
        const int in_len = static_cast<int>(strlen(in));

        int uni_len = ::MultiByteToWideChar(CP_ACP, 0, in, in_len, nullptr, 0);
        if (uni_len > 0)
        {
            std::wstring uni;
            uni.resize(uni_len);
            if (::MultiByteToWideChar(CP_ACP, 0, in, in_len, &uni[0], uni_len) != 0)
            {
                return uni;
            }
        }
    }

    return std::wstring();
}

std::string ANSIfromUNICODE(const std::wstring& in)
{
    int len = ::WideCharToMultiByte(CP_ACP, 0, in.data(), in.length(), nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        std::string acp;
        acp.resize(len);
        if (::WideCharToMultiByte(CP_ACP, 0, in.data(), in.length(), &acp[0], acp.length(), nullptr, nullptr) != 0)
        {
            return acp;
        }
    }

    return std::string();
}

std::string ANSIfromUNICODE(const WCHAR* in)
{
    if (in && in[0] != 0)
    {
        const int uni_len = static_cast<int>(wcslen(in));

        int acp_len = ::WideCharToMultiByte(CP_ACP, 0, in, uni_len, nullptr, 0, nullptr, nullptr);
        if (acp_len > 0)
        {
            std::string acp;
            acp.resize(acp_len);
            if (::WideCharToMultiByte(CP_ACP, 0, in, uni_len, &acp[0], acp.length(), nullptr, nullptr) != 0)
            {
                return acp;
            }
        }
    }

    return std::string();
}

std::string UTF8fromUNICODE(const std::wstring& in)
{
    int len = ::WideCharToMultiByte(CP_UTF8, 0, in.data(), in.length(), nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        std::string utf8;
        utf8.resize(len);
        if (::WideCharToMultiByte(CP_UTF8, 0, in.data(), in.length(), &utf8[0], len, nullptr, nullptr) != 0)
        {
            return utf8;
        }
    }

    return std::string();
}

std::string UTF8fromUNICODE(const WCHAR* in)
{
    if (in && in[0] != 0)
    {
        const int uni_len = static_cast<int>(wcslen(in));

        int utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, in, uni_len, nullptr, 0, nullptr, nullptr);
        if (utf8_len > 0)
        {
            std::string utf8;
            utf8.resize(utf8_len);
            if (::WideCharToMultiByte(CP_UTF8, 0, in, uni_len, &utf8[0], utf8_len, nullptr, nullptr) != 0)
            {
                return utf8;
            }
        }
    }

    return std::string();
}

std::wstring UNICODEfromUTF8(const std::string& in)
{
    int len = ::MultiByteToWideChar(CP_UTF8, 0, in.data(), in.length(), nullptr, 0);
    if (len > 0)
    {
        std::wstring uni;
        uni.resize(len);
        if (::MultiByteToWideChar(CP_UTF8, 0, in.data(), in.length(), &uni[0], len) != 0)
        {
            return uni;
        }
    }

    return std::wstring();
}

std::wstring UNICODEfromUTF8(const char* in)
{
    if (in && in[0] != 0)
    {
        const int utf8_len = static_cast<int>(strlen(in));

        int utf16_len = ::MultiByteToWideChar(CP_UTF8, 0, in, utf8_len, nullptr, 0);
        if (utf16_len > 0)
        {
            std::wstring uni;
            uni.resize(utf16_len);
            if (::MultiByteToWideChar(CP_UTF8, 0, in, utf8_len, &uni[0], utf16_len) != 0)
            {
                return uni;
            }
        }
    }

    return std::wstring();
}

std::string ANSItoUTF8(const std::string& in)
{
    return UTF8fromUNICODE(UNICODEfromANSI(in));
}

std::string ANSItoUTF8(const char* in)
{
    return UTF8fromUNICODE(UNICODEfromANSI(in));
}

std::string UTF8toANSI(const std::string& in)
{
    return ANSIfromUNICODE(UNICODEfromUTF8(in));
}

std::string UTF8toANSI(const char* in)
{
    return ANSIfromUNICODE(UNICODEfromUTF8(in));
}

} // namespace aspia
