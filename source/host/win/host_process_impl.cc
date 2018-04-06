//
// PROJECT:         Aspia
// FILE:            host/win/host_process_impl.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/win/host_process_impl.h"

#include <QDebug>

#include <userenv.h>
#include <wtsapi32.h>

#include "base/system_error_code.h"

namespace aspia {

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        qWarning() << "OpenProcessToken failed: " << lastSystemErrorString();
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        qWarning() << "DuplicateTokenEx failed: " << lastSystemErrorString();
        return false;
    }

    return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool createPrivilegedToken(ScopedHandle* token_out)
{
    ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        qWarning() << "LookupPrivilegeValueW failed: " << lastSystemErrorString();
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0,
                               nullptr, nullptr))
    {
        qWarning() << "AdjustTokenPrivileges failed: " << lastSystemErrorString();
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool createSessionToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
        return false;

    ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        qWarning() << "ImpersonateLoggedOnUser failed: " << lastSystemErrorString();
        return false;
    }

    // Change the session ID of the token.
    BOOL ret = SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id));

    if (!RevertToSelf())
    {
        qFatal("RevertToSelf failed");
        return false;
    }

    if (!ret)
    {
        qWarning() << "SetTokenInformation failed: " << lastSystemErrorString();
        return false;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        qWarning() << "SetTokenInformation failed: " << lastSystemErrorString();
        return false;
    }

    token_out->reset(session_token.release());
    return true;
}

bool createLoggedOnUserToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        qWarning() << "ImpersonateLoggedOnUser failed: " << lastSystemErrorString();
        return false;
    }

    BOOL ret = WTSQueryUserToken(session_id, token_out->recieve());

    if (!RevertToSelf())
    {
        qFatal("RevertToSelf failed");
        return false;
    }

    if (!ret)
    {
        qWarning() << "WTSQueryUserToken failed: " << lastSystemErrorString();
        return false;
    }

    return true;
}

QString createCommandLine(const QString& program, const QStringList& arguments)
{
    QString args;

    if (!program.isEmpty())
    {
        QString program_name = program;

        if (!program_name.startsWith('\"') && !program_name.endsWith('\"') &&
            program_name.contains(' '))
        {
            program_name = '\"' + program_name + '\"';
        }

        program_name.replace('/', '\\');

        args = program_name + ' ';
    }

    for (int i = 0; i < arguments.size(); ++i)
    {
        QString tmp = arguments.at(i);

        // Quotes are escaped and their preceding backslashes are doubled.
        tmp.replace(QRegExp(QStringLiteral("(\\\\*)\"")), QStringLiteral("\\1\\1\\\""));

        if (tmp.isEmpty() || tmp.contains(' ') || tmp.contains('\t'))
        {
            // The argument must not end with a \ since this would be interpreted as escaping the
            // quote -- rather put the \ behind the quote: e.g. rather use "foo"\ than "foo\"
            int len = tmp.length();
            while (len > 0 && tmp.at(len - 1) == '\\')
                --len;

            tmp.insert(len, '"');
            tmp.prepend('"');
        }

        args += ' ' + tmp;
    }

    return args;
}

} // namespace

HostProcessImpl::HostProcessImpl(HostProcess* process)
    : process_(process)
{
    // Nothing
}

void HostProcessImpl::startProcess()
{
    state_ = HostProcess::Starting;

    ScopedHandle session_token;

    if (account_ == HostProcess::System)
    {
        if (!createSessionToken(session_id_, &session_token))
        {
            state_ = HostProcess::NotRunning;
            process_->processError();
            return;
        }
    }
    else
    {
        Q_ASSERT(account_ == HostProcess::User);

        if (!createLoggedOnUserToken(session_id_, &session_token))
        {
            state_ = HostProcess::NotRunning;
            process_->processError();
            return;
        }
    }

    if (!startProcessWithToken(session_token))
    {
        state_ = HostProcess::NotRunning;
        process_->processError();
        return;
    }

    finish_notifier_ = new QWinEventNotifier(process_handle_.get());

    QObject::connect(finish_notifier_, &QWinEventNotifier::activated, [this](HANDLE /* handle */)
    {
        state_ = HostProcess::NotRunning;
        process_->processFinished();
    });

    finish_notifier_->setEnabled(true);

    state_ = HostProcess::Running;
}

void HostProcessImpl::killProcess()
{
    if (process_handle_.isValid())
        TerminateProcess(process_handle_, 0);
}

bool HostProcessImpl::startProcessWithToken(HANDLE token)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    PVOID environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        qWarning() << "CreateEnvironmentBlock failed: " << lastSystemErrorString();
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    QString command_line = createCommandLine(program_, arguments_);

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<LPWSTR>(reinterpret_cast<const wchar_t*>(
                                  command_line.utf16())),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        qWarning() << "CreateProcessAsUserW failed: " << lastSystemErrorString();
        DestroyEnvironmentBlock(environment);
        return false;
    }

    thread_handle_.reset(process_info.hThread);
    process_handle_.reset(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace aspia
