//
// PROJECT:         Aspia
// FILE:            protocol/authorization.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__AUTHORIZATION_H
#define _ASPIA_PROTOCOL__AUTHORIZATION_H

#include <string>

namespace aspia {

bool IsValidUserName(std::wstring_view username);
bool IsValidPassword(std::wstring_view password);
bool IsValidUserNameUTF8(std::string_view username);
bool IsValidPasswordUTF8(std::string_view password);

std::string CreatePasswordHash(const std::wstring& password);
bool IsValidPasswordHash(const std::string& password_hash);

std::string CreateNonce();
std::string CreateUserKey(const std::wstring& username,
                          const std::string& password_hash,
                          const std::string& nonce);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__AUTHORIZATION_H
