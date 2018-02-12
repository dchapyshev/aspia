//
// PROJECT:         Aspia
// FILE:            network/url.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__URL_H
#define _ASPIA_NETWORK__URL_H

#include <string>

namespace aspia {

class URL
{
public:
    URL(const URL& other);
    URL& operator=(const URL& other);

    URL(URL&& other);
    URL& operator=(URL&& other);

    ~URL() = default;

    static URL FromUTF8String(const std::string& utf8_url_string);
    static URL FromString(const std::wstring& url_string);
    static URL FromCString(const wchar_t* url_string);

    bool IsValid() const;
    bool Open() const;

private:
    URL() = default;
    URL(const std::wstring& url_string);

    std::wstring url_;
};

} // namespace aspia

#endif // _ASPIA_NETWORK__URL_H
