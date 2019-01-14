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

#include "base/win/process_util.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "base/logging.h"

namespace base::win {

namespace {

QString normalizedProgram(const QString& program)
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

QString createParamaters(const QStringList& arguments)
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

} // namespace

bool executeProcess(const QString& program, const QStringList& arguments, ProcessExecuteMode mode)
{
    QString normalized_program = normalizedProgram(program);
    QString parameters = createParamaters(arguments);

    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));

    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = ((mode == ProcessExecuteMode::ELEVATE) ? L"runas" : L"open");
    sei.lpFile       = reinterpret_cast<const wchar_t*>(normalized_program.utf16());
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = reinterpret_cast<const wchar_t*>(parameters.utf16());

    if (!ShellExecuteExW(&sei))
    {
        PLOG(LS_WARNING) << "ShellExecuteExW failed";
        return false;
    }

    return true;
}

bool executeProcess(const QString& program, ProcessExecuteMode mode)
{
    return executeProcess(program, QStringList(), mode);
}

bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        PLOG(LS_WARNING) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        PLOG(LS_WARNING) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

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
        PLOG(LS_WARNING) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0,
                               nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
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

} // namespace base::win
