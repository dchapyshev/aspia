//
// PROJECT:         Aspia
// FILE:            network/url.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/url.h"

#include <shellapi.h>

#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

URL::URL(const std::wstring& url_string)
    : url_(url_string)
{
    // Nothing
}

URL::URL(const URL& other)
    : url_(other.url_)
{
    // Nothing
}

URL& URL::operator=(const URL& other)
{
    if (this == &other)
        return *this;

    url_ = other.url_;
    return *this;
}

URL::URL(URL&& other)
    : url_(std::move(other.url_))
{
    // Nothing
}

URL& URL::operator=(URL&& other)
{
    url_ = std::move(other.url_);
    return *this;
}

// static
URL URL::FromUTF8String(const std::string& utf8_url_string)
{
    return URL(UNICODEfromUTF8(utf8_url_string));
}

// static
URL URL::FromString(const std::wstring& url_string)
{
    return URL(url_string);
}

// static
URL URL::FromCString(const wchar_t* url_string)
{
    if (!url_string)
        return URL();

    return URL(url_string);
}

bool URL::IsValid() const
{
    return !url_.empty();
}

bool URL::Open() const
{
    if (!IsValid())
        return false;

    int ret = reinterpret_cast<int>(
        ShellExecuteW(nullptr, L"open", url_.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (ret <= 32)
    {
        DLOG(LS_WARNING) << "ShellExecuteW failed: " << ret;
        return false;
    }

    return true;
}

} // namespace aspia
