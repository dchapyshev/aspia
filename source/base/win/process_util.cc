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

namespace aspia {

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

} // namespace aspia
