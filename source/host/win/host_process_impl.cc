//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/win/host_process_impl.h"

#include <userenv.h>
#include <wtsapi32.h>

#include "base/win/process_util.h"
#include "base/logging.h"

namespace host {

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
HostProcess::ErrorCode createSessionToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
        return HostProcess::OtherError;

    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return HostProcess::OtherError;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        PLOG(LS_WARNING) << "ImpersonateLoggedOnUser failed";
        return HostProcess::OtherError;
    }

    // Change the session ID of the token.
    BOOL result = SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id));

    BOOL ret = RevertToSelf();
    CHECK(ret);

    if (!result)
    {
        PLOG(LS_WARNING) << "SetTokenInformation failed";
        return HostProcess::OtherError;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(LS_WARNING) << "SetTokenInformation failed";
        return HostProcess::OtherError;
    }

    token_out->reset(session_token.release());
    return HostProcess::NoError;
}

HostProcess::ErrorCode createLoggedOnUserToken(
    DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return HostProcess::OtherError;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        PLOG(LS_WARNING) << "ImpersonateLoggedOnUser failed";
        return HostProcess::OtherError;
    }

    HostProcess::ErrorCode error_code = HostProcess::NoError;

    base::win::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        DWORD system_error_code = GetLastError();

        if (system_error_code != ERROR_NO_TOKEN)
        {
            error_code = HostProcess::OtherError;
            LOG(LS_WARNING) << "WTSQueryUserToken failed: "
                            << base::systemErrorCodeToString(system_error_code);
        }
        else
        {
            error_code = HostProcess::NoLoggedOnUser;
        }
    }

    BOOL ret = RevertToSelf();
    CHECK(ret);

    if (error_code == HostProcess::NoError)
    {
        TOKEN_ELEVATION_TYPE elevation_type;
        DWORD returned_length;

        if (!GetTokenInformation(user_token,
                                 TokenElevationType,
                                 &elevation_type,
                                 sizeof(elevation_type),
                                 &returned_length))
        {
            PLOG(LS_WARNING) << "GetTokenInformation failed";
            return HostProcess::OtherError;
        }

        switch (elevation_type)
        {
            // The token is a limited token.
            case TokenElevationTypeLimited:
            {
                TOKEN_LINKED_TOKEN linked_token_info;

                // Get the unfiltered token for a silent UAC bypass.
                if (!GetTokenInformation(user_token,
                                         TokenLinkedToken,
                                         &linked_token_info,
                                         sizeof(linked_token_info),
                                         &returned_length))
                {
                    PLOG(LS_WARNING) << "GetTokenInformation failed";
                    return HostProcess::OtherError;
                }

                // Attach linked token.
                token_out->reset(linked_token_info.LinkedToken);
            }
            break;

            case TokenElevationTypeDefault: // The token does not have a linked token.
            case TokenElevationTypeFull:    // The token is an elevated token.
            default:
                token_out->reset(user_token.release());
                break;
        }

        DWORD ui_access = 1;
        if (!SetTokenInformation(token_out->get(), TokenUIAccess, &ui_access, sizeof(ui_access)))
        {
            PLOG(LS_WARNING) << "SetTokenInformation failed";
            return HostProcess::OtherError;
        }
    }

    return error_code;
}

BOOL CALLBACK terminateEnumProc(HWND hwnd, LPARAM lparam)
{
    DWORD process_id = static_cast<DWORD>(lparam);

    DWORD current_process_id = 0;
    GetWindowThreadProcessId(hwnd, &current_process_id);

    if (process_id == current_process_id)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);

    return TRUE;
}

} // namespace

HostProcessImpl::HostProcessImpl(HostProcess* process)
    : process_(process)
{
    // Nothing
}

HostProcessImpl::~HostProcessImpl()
{
    delete finish_notifier_;
}

void HostProcessImpl::startProcess()
{
    state_ = HostProcess::Starting;

    base::win::ScopedHandle session_token;

    if (account_ == HostProcess::System)
    {
        HostProcess::ErrorCode error_code = createSessionToken(session_id_, &session_token);
        if (error_code != HostProcess::NoError)
        {
            state_ = HostProcess::NotRunning;
            emit process_->errorOccurred(error_code);
            return;
        }
    }
    else
    {
        DCHECK_EQ(account_, HostProcess::User);

        HostProcess::ErrorCode error_code = createLoggedOnUserToken(session_id_, &session_token);
        if (error_code != HostProcess::NoError)
        {
            state_ = HostProcess::NotRunning;
            emit process_->errorOccurred(error_code);
            return;
        }
    }

    if (!startProcessWithToken(session_token))
    {
        state_ = HostProcess::NotRunning;
        emit process_->errorOccurred(HostProcess::OtherError);
        return;
    }

    finish_notifier_ = new QWinEventNotifier(process_handle_.get());

    QObject::connect(finish_notifier_, &QWinEventNotifier::activated, [this](HANDLE /* handle */)
    {
        state_ = HostProcess::NotRunning;
        emit process_->finished();
    });

    finish_notifier_->setEnabled(true);

    state_ = HostProcess::Running;
}

void HostProcessImpl::killProcess()
{
    if (!process_handle_.isValid())
    {
        LOG(LS_WARNING) << "Invalid process handle";
        return;
    }

    TerminateProcess(process_handle_, 0);
}

void HostProcessImpl::terminateProcess()
{
    if (!process_handle_.isValid())
    {
        LOG(LS_WARNING) << "Invalid process handle";
        return;
    }

    EnumWindows(terminateEnumProc, process_id_);
    PostThreadMessageW(thread_id_, WM_CLOSE, 0, 0);
}

bool HostProcessImpl::startProcessWithToken(HANDLE token)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(LS_WARNING) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    QString command_line = base::win::createCommandLine(program_, arguments_);

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<wchar_t*>(qUtf16Printable(command_line)),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(LS_WARNING) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    thread_handle_.reset(process_info.hThread);
    process_handle_.reset(process_info.hProcess);
    thread_id_ = process_info.dwThreadId;
    process_id_ = process_info.dwProcessId;

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace host
