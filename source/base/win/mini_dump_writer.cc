//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/mini_dump_writer.h"

#include "base/files/base_paths.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <strsafe.h>

namespace base {

namespace {

wchar_t g_file_prefix[32] = { 0 };

//--------------------------------------------------------------------------------------------------
LONG WINAPI exceptionFilter(EXCEPTION_POINTERS* exception_pointers)
{
    wchar_t file_dir[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, file_dir);
    StringCbCatW(file_dir, sizeof(file_dir), L"\\aspia");

    if (!(GetFileAttributesW(file_dir) & FILE_ATTRIBUTE_DIRECTORY))
        CreateDirectoryW(file_dir, nullptr);

    SYSTEMTIME time;
    GetLocalTime(&time);

    wchar_t file_name[MAX_PATH] = { 0 };
    StringCbPrintfW(file_name,
                    sizeof(file_name),
                    L"%s\\%s-%04d%02d%02d-%02d%02d%02d.%03d.dmp",
                    file_dir,
                    g_file_prefix,
                    time.wYear, time.wMonth, time.wDay,
                    time.wHour, time.wMinute, time.wSecond,
                    time.wMilliseconds);

    HANDLE dump_file = CreateFileW(file_name,
                                   GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_WRITE | FILE_SHARE_READ,
                                   nullptr,
                                   CREATE_ALWAYS,
                                   0,
                                   nullptr);

    MINIDUMP_EXCEPTION_INFORMATION exception_information;
    exception_information.ThreadId = GetCurrentThreadId();
    exception_information.ExceptionPointers = exception_pointers;
    exception_information.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(),
                      GetCurrentProcessId(),
                      dump_file,
                      MiniDumpWithFullMemory,
                      &exception_information,
                      nullptr,
                      nullptr);

    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void installFailureHandler()
{
    std::filesystem::path exec_file_path;
    if (!base::BasePaths::currentExecFile(&exec_file_path))
        return;

    std::filesystem::path exec_file_name = exec_file_path.filename();
    exec_file_name.replace_extension();

    StringCbCopyW(g_file_prefix, sizeof(g_file_prefix), exec_file_name.wstring().c_str());
    SetUnhandledExceptionFilter(exceptionFilter);
}

} // namespace base
