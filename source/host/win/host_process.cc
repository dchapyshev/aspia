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

#include "host/win/host_process.h"

#include <userenv.h>
#include <wtsapi32.h>

#include "base/win/process.h"
#include "base/win/process_util.h"
#include "base/win/scoped_impersonator.h"
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

    base::win::ScopedImpersonator impersonator;

    if (!impersonator.loggedOnUser(privileged_token))
        return HostProcess::OtherError;

    // Change the session ID of the token.
    if (!SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id)))
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

    base::win::ScopedImpersonator impersonator;

    if (!impersonator.loggedOnUser(privileged_token))
        return HostProcess::OtherError;

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

    impersonator.revertToSelf();

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

bool startProcessWithToken(HANDLE token,
                           const QString& command_line,
                           base::win::ScopedHandle* process,
                           base::win::ScopedHandle* thread)
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

    thread->reset(process_info.hThread);
    process->reset(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace

HostProcess::HostProcess(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostProcess::~HostProcess() = default;

HostProcess::ErrorCode HostProcess::start()
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::start: Process is already running";
        return HostProcess::OtherError;
    }

    state_ = HostProcess::Starting;

    base::win::ScopedHandle session_token;
    HostProcess::ErrorCode error_code;

    if (account_ == HostProcess::System)
    {
        error_code = createSessionToken(session_id_, &session_token);
    }
    else
    {
        DCHECK_EQ(account_, HostProcess::User);
        error_code = createLoggedOnUserToken(session_id_, &session_token);
    }

    if (error_code != HostProcess::NoError)
    {
        state_ = HostProcess::NotRunning;
        return error_code;
    }

    QString command_line = base::win::Process::createCommandLine(program_, arguments_);

    base::win::ScopedHandle process_handle;
    base::win::ScopedHandle thread_handle;

    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
    {
        state_ = HostProcess::NotRunning;
        return HostProcess::OtherError;
    }

    process_ = new base::win::Process(process_handle.release(), thread_handle.release(), this);

    connect(process_, &base::win::Process::finished, [this]()
    {
        state_ = HostProcess::NotRunning;
        emit finished();
    });

    state_ = HostProcess::Running;
    return HostProcess::NoError;
}

HostProcess::ErrorCode HostProcess::start(base::win::SessionId session_id,
                                          Account account,
                                          const QString& program,
                                          const QStringList& arguments)
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::start: Process is already running";
        return HostProcess::OtherError;
    }

    session_id_ = session_id;
    account_ = account;
    program_ = program;
    arguments_ = arguments;

    return start();
}

base::win::SessionId HostProcess::sessionId() const
{
    return session_id_;
}

void HostProcess::setSessionId(base::win::SessionId session_id)
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::setSessionId: Process is already running";
        return;
    }

    session_id_ = session_id;
}

HostProcess::Account HostProcess::account() const
{
    return account_;
}

void HostProcess::setAccount(Account account)
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::setAccount: Process is already running";
        return;
    }

    account_ = account;
}

QString HostProcess::program() const
{
    return program_;
}

void HostProcess::setProgram(const QString& program)
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::setProgram: Process is already running";
        return;
    }

    program_ = program;
}

QStringList HostProcess::arguments() const
{
    return arguments_;
}

void HostProcess::setArguments(const QStringList& arguments)
{
    if (state_ != NotRunning)
    {
        DLOG(LS_WARNING) << "HostProcess::setArguments: Process is already running";
        return;
    }

    arguments_ = arguments;
}

HostProcess::ProcessState HostProcess::state() const
{
    return state_;
}

void HostProcess::kill()
{
    if (process_)
        process_->kill();
}

void HostProcess::terminate()
{
    if (process_)
        process_->terminate();
}

} // namespace host
