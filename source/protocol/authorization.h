//
// PROJECT:         Aspia
// FILE:            protocol/authorization.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__AUTHORIZATION_H
#define _ASPIA_PROTOCOL__AUTHORIZATION_H

#include <string>

namespace aspia {

bool IsValidUserName(const std::wstring& username);
bool IsValidPassword(const std::wstring& password);

std::string CreatePasswordHash(const std::wstring& password);
bool IsValidPasswordHash(const std::string& password_hash);

std::string CreateNonce();
std::string CreateUserKey(const std::string& password_hash, const std::string& nonce);

} // namespace aspia

#endif // _ASPIA_PROTOCOL__AUTHORIZATION_H
