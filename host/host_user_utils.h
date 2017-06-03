//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user_utils.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_UTILS_H
#define _ASPIA_HOST__HOST_USER_UTILS_H

#include "proto/host_user.pb.h"

namespace aspia {

// Number of iterations for hashing a user's password.
static const size_t kUserPasswordHashIterCount = 100;

bool IsValidUserName(const std::wstring& username);

bool IsValidPassword(const std::wstring& password);

bool IsValidUser(const proto::HostUser& user);

bool IsValidUserList(const proto::HostUserList& list);

bool WriteUserList(const proto::HostUserList& list);

bool ReadUserList(proto::HostUserList& list);

bool IsUniqueUserName(const proto::HostUserList& list,
                      const std::wstring& username);

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_UTILS_H
