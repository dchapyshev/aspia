//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/process.h"
#include "base/logging.h"
#include "base/win/process_util.h"
#include "base/win/scoped_impersonator.h"

#include <psapi.h>
#include <tlhelp32.h>

namespace base::win {

namespace {

// Creates a copy of the current process with |privilege_name| privilege enabled.
bool createToken(const wchar_t* privilege_name, win::ScopedHandle* token_out)
{
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    win::ScopedHandle privileged_token;
    if (!copyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_WARNING) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
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

Process::Process(ProcessId process_id, QObject* parent)
    : QObject(parent)
{
    // We need SE_DEBUG_NAME privilege to open the process.
    ScopedHandle privileged_token;
    if (!createToken(SE_DEBUG_NAME, &privileged_token))
        return;

    ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
        return;

    ScopedHandle process(OpenProcess(PROCESS_ALL_ACCESS, TRUE, process_id));
    if (!process.isValid())
    {
        PLOG(LS_WARNING) << "OpenProcess failed";
        return;
    }

    process_.reset(process.release());
    initNotifier();

    state_ = State::STARTED;
}

Process::Process(HANDLE process, HANDLE thread, QObject* parent)
    : QObject(parent),
      process_(process),
      thread_(thread)
{
    if (!process_.isValid())
        return;

    initNotifier();

    state_ = State::STARTED;
}

Process::~Process() = default;

// static
QString Process::createCommandLine(const QString& program, const QStringList& arguments)
{
    QString args;

    if (!program.isEmpty())
        args = normalizedProgram(program) + QLatin1Char(' ');

    args += createParamaters(arguments);
    return args;
}

// static
QString Process::normalizedProgram(const QString& program)
{
    QString normalized = program;

    if (!normalized.startsWith(QLatin1Char('\"')) &&
        !normalized.endsWith(QLatin1Char('\"')) &&
        normalized.contains(QLatin1Char(' ')))
    {
        normalized = QLatin1Char('\"') + normalized + QLatin1Char('\"');
    }

    normalized.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return normalized;
}

// static
QString Process::createParamaters(const QStringList& arguments)
{
    QString parameters;

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

        parameters += QLatin1Char(' ') + tmp;
    }

    return parameters;
}

bool Process::isValid() const
{
    return process_.isValid();
}

QString Process::filePath() const
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameExW(process_.get(), nullptr, buffer, _countof(buffer)))
    {
        PLOG(LS_WARNING) << "GetModuleFileNameExW failed";
        return QString();
    }

    return QString::fromUtf16(reinterpret_cast<const ushort*>(buffer));
}

QString Process::fileName() const
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (GetProcessImageFileNameW(process_.get(), buffer, _countof(buffer)))
    {
        PLOG(LS_WARNING) << "GetProcessImageFileNameW failed";
        return QString();
    }

    return QString::fromUtf16(reinterpret_cast<const ushort*>(buffer));
}

ProcessId Process::processId() const
{
    return GetProcessId(process_.get());
}

SessionId Process::sessionId() const
{
    DWORD session_id;

    if (!ProcessIdToSessionId(processId(), &session_id))
        return kInvalidSessionId;

    return session_id;
}

int Process::exitCode() const
{
    DWORD exit_code;

    if (!GetExitCodeProcess(process_.get(), &exit_code))
    {
        PLOG(LS_WARNING) << "GetExitCodeProcess failed";
        return 0;
    }

    return exit_code;
}

void Process::kill()
{
    if (!process_.isValid())
    {
        LOG(LS_WARNING) << "Invalid process handle";
        return;
    }

    TerminateProcess(process_, 0);
}

void Process::terminate()
{
    if (!process_.isValid())
    {
        LOG(LS_WARNING) << "Invalid process handle";
        return;
    }

    ProcessId process_id = processId();

    EnumWindows(terminateEnumProc, process_id);

    if (!thread_.isValid())
    {
        ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, process_id));
        if (!snapshot.isValid())
        {
            PLOG(LS_WARNING) << "CreateToolhelp32Snapshot failed";
            return;
        }

        THREADENTRY32 entry;
        entry.dwSize = sizeof(entry);

        for (BOOL ret = Thread32First(snapshot, &entry);
             ret && GetLastError() != ERROR_NO_MORE_FILES;
             ret = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID == process_id)
            {
                if (!PostThreadMessageW(entry.th32ThreadID, WM_CLOSE, 0, 0))
                {
                    PLOG(LS_WARNING) << "PostThreadMessageW failed";
                }
            }
        }
    }
    else
    {
        PostThreadMessageW(GetThreadId(thread_), WM_CLOSE, 0, 0);
    }
}

void Process::initNotifier()
{
    notifier_ = new QWinEventNotifier(process_, this);

    connect(notifier_, &QWinEventNotifier::activated, [this](HANDLE process_handle)
    {
        DCHECK_EQ(process_handle, process_.get());

        if (state_ != State::STARTED)
            return;

        state_ = State::FINISHED;
        emit finished(exitCode());
    });

    notifier_->setEnabled(true);
}

} // namespace base::win
