//
// PROJECT:         Aspia
// FILE:            host/win/host_process_impl.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/win/host_process_impl.h"

#include <QDebug>

#include <userenv.h>
#include <wtsapi32.h>

#include "base/errno_logging.h"

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
        qWarningErrno("OpenProcessToken failed");
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        qWarningErrno("DuplicateTokenEx failed");
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
        qWarningErrno("LookupPrivilegeValueW failed");
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0,
                               nullptr, nullptr))
    {
        qWarningErrno("AdjustTokenPrivileges failed");
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
HostProcess::ErrorCode createSessionToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
        return HostProcess::OtherError;

    ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return HostProcess::OtherError;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        qWarningErrno("ImpersonateLoggedOnUser failed");
        return HostProcess::OtherError;
    }

    // Change the session ID of the token.
    BOOL ret = SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id));

    if (!RevertToSelf())
    {
        qFatal("RevertToSelf failed");
        return HostProcess::OtherError;
    }

    if (!ret)
    {
        qWarningErrno("SetTokenInformation failed");
        return HostProcess::OtherError;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        qWarningErrno("SetTokenInformation failed");
        return HostProcess::OtherError;
    }

    token_out->reset(session_token.release());
    return HostProcess::NoError;
}

HostProcess::ErrorCode createLoggedOnUserToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return HostProcess::OtherError;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        qWarningErrno("ImpersonateLoggedOnUser failed");
        return HostProcess::OtherError;
    }

    HostProcess::ErrorCode error_code = HostProcess::NoError;

    if (!WTSQueryUserToken(session_id, token_out->recieve()))
    {
        DWORD system_error_code = GetLastError();

        if (system_error_code != ERROR_NO_TOKEN)
        {
            error_code = HostProcess::OtherError;
            qWarning() << "WTSQueryUserToken failed: " << errnoToString(system_error_code);
        }
        else
        {
            error_code = HostProcess::NoLoggedOnUser;
        }
    }

    if (!RevertToSelf())
    {
        qFatalErrno("RevertToSelf failed");
        return HostProcess::OtherError;
    }

    return error_code;
}

QString createCommandLine(const QString& program, const QStringList& arguments)
{
    QString args;

    if (!program.isEmpty())
    {
        QString program_name = program;

        if (!program_name.startsWith(QLatin1Char('\"')) &&
            !program_name.endsWith(QLatin1Char('\"')) &&
            program_name.contains(QLatin1Char(' ')))
        {
            program_name = QLatin1Char('\"') + program_name + QLatin1Char('\"');
        }

        program_name.replace(QLatin1Char('/'), QLatin1Char('\\'));

        args = program_name + QLatin1Char(' ');
    }

    for (int i = 0; i < arguments.size(); ++i)
    {
        QString tmp = arguments.at(i);

        // Quotes are escaped and their preceding backslashes are doubled.
        tmp.replace(QRegExp(QLatin1String("(\\\\*)\"")), QLatin1String("\\1\\1\\\""));

        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t')))
        {
            // The argument must not end with a \ since this would be interpreted as escaping the
            // quote -- rather put the \ behind the quote: e.g. rather use "foo"\ than "foo\"
            int len = tmp.length();
            while (len > 0 && tmp.at(len - 1) == QLatin1Char('\\'))
                --len;

            tmp.insert(len, QLatin1Char('\"'));
            tmp.prepend(QLatin1Char('\"'));
        }

        args += QLatin1Char(' ') + tmp;
    }

    return args;
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

    ScopedHandle session_token;

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
        Q_ASSERT(account_ == HostProcess::User);

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
    if (process_handle_.isValid())
        TerminateProcess(process_handle_, 0);
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
        qWarningErrno("CreateEnvironmentBlock failed");
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    QString command_line = createCommandLine(program_, arguments_);

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
        qWarningErrno("CreateProcessAsUserW failed");
        DestroyEnvironmentBlock(environment);
        return false;
    }

    thread_handle_.reset(process_info.hThread);
    process_handle_.reset(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace aspia
