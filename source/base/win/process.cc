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

#include <psapi.h>

#include "base/logging.h"

namespace base::win {

namespace {

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

Process::Process(uint32_t process_id, QObject* parent)
    : QObject(parent)
{
    ScopedHandle process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));
    if (process.isValid())
        return;



    process_.reset(process.release());
}

Process::Process(HANDLE process, HANDLE thread, QObject* parent)
    : QObject(parent),
      process_(process),
      thread_(thread)
{
    notifier_ = new QWinEventNotifier(process_.get(), this);
    connect(notifier_, &QWinEventNotifier::activated, this, &Process::finished);
    notifier_->setEnabled(true);
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
    return process_.isValid() && thread_.isValid();
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

uint32_t Process::processId() const
{
    return GetProcessId(process_.get());
}

uint32_t Process::sessionId() const
{
    DWORD session_id;

    if (!ProcessIdToSessionId(processId(), &session_id))
        return -1;

    return session_id;
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

    EnumWindows(terminateEnumProc, processId());
    PostThreadMessageW(GetThreadId(thread_), WM_CLOSE, 0, 0);
}

} // namespace base::win
