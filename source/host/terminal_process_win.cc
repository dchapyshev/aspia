//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/terminal_process_win.h"

#include <string>

#include <qt_windows.h>
#include <comdef.h>

#include <asio/buffer.hpp>
#include <asio/error.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/win/windows_version.h"

// The ConPTY declarations are only provided for the Windows 10 SDK target, but the project keeps a
// Windows 7 baseline. HPCON is just a void* handle and the only missing constant is defined here; the
// ConPTY functions are resolved at run time via GetProcAddress (so the binary still loads on Windows 7).
#if !defined(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE)
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

namespace {

using CreatePseudoConsoleFunc = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, void**);
using ResizePseudoConsoleFunc = HRESULT (WINAPI*)(void*, COORD);
using ClosePseudoConsoleFunc = VOID (WINAPI*)(void*);

//--------------------------------------------------------------------------------------------------
template <typename T>
T conPtyFunction(const char* name)
{
    HMODULE module = GetModuleHandleW(L"kernel32.dll");
    if (!module)
        return nullptr;
    return reinterpret_cast<T>(GetProcAddress(module, name));
}

//--------------------------------------------------------------------------------------------------
// Creates a unidirectional pipe whose read end supports overlapped I/O (required to read it through
// an asio stream). CreatePipe can not produce overlapped handles, so a uniquely named pipe is used.
bool createOverlappedPipe(ScopedHandle* read_end, ScopedHandle* write_end)
{
    static unsigned counter = 0;

    // Headroom for what ConPTY may write between the wakeups of the agent's I/O context; doRead()
    // drains the pipe completely on each wakeup, so this only needs to cover the scheduling latency.
    const DWORD buffer_size = 64 * 1024;

    const std::wstring name = L"\\\\.\\pipe\\aspia.conpty." +
        std::to_wstring(GetCurrentProcessId()) + L'.' + std::to_wstring(counter++);

    read_end->reset(CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, buffer_size, buffer_size,
        0, nullptr));
    if (!read_end->isValid())
    {
        PLOG(ERROR) << "CreateNamedPipeW failed";
        return false;
    }

    write_end->reset(CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!write_end->isValid())
    {
        PLOG(ERROR) << "CreateFileW failed";
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool TerminalProcessWin::isSupported()
{
    // ConPTY is available starting with Windows 10 1809 (build 17763).
    return windowsVersion() >= VERSION_WIN10_RS5;
}

//--------------------------------------------------------------------------------------------------
// static
void TerminalProcessWin::PseudoConsoleTraits::close(void* pseudo_console)
{
    if (!isValid(pseudo_console))
        return;

    static ClosePseudoConsoleFunc func = conPtyFunction<ClosePseudoConsoleFunc>("ClosePseudoConsole");
    if (func)
        func(pseudo_console);
}

//--------------------------------------------------------------------------------------------------
TerminalProcessWin::TerminalProcessWin(QObject* parent)
    : TerminalProcess(parent),
      output_stream_(AsioEventDispatcher::ioContext())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TerminalProcessWin::~TerminalProcessWin()
{
    LOG(INFO) << "Dtor";
    stop();
}

//--------------------------------------------------------------------------------------------------
bool TerminalProcessWin::start(int columns, int rows)
{
    static CreatePseudoConsoleFunc create_pseudo_console =
        conPtyFunction<CreatePseudoConsoleFunc>("CreatePseudoConsole");
    if (!create_pseudo_console)
    {
        LOG(ERROR) << "ConPTY is not available (requires Windows 10 1809 or newer)";
        return false;
    }

    // Input pipe: synchronous, keyboard input is written to |input_write|.
    ScopedHandle input_read;
    ScopedHandle input_write;
    if (!CreatePipe(input_read.recieve(), input_write.recieve(), nullptr, 0))
    {
        PLOG(ERROR) << "CreatePipe failed";
        return false;
    }

    // Output pipe: the read end is overlapped so the shell output can be read asynchronously.
    ScopedHandle output_read;
    ScopedHandle output_write;
    if (!createOverlappedPipe(&output_read, &output_write))
        return false;

    COORD size;
    size.X = static_cast<SHORT>(columns);
    size.Y = static_cast<SHORT>(rows);

    decltype(pseudo_console_) pseudo_console;
    _com_error hr = create_pseudo_console(size, input_read, output_write, 0, pseudo_console.recieve());
    if (FAILED(hr.Error()))
    {
        LOG(ERROR) << "CreatePseudoConsole failed:" << hr;
        return false;
    }

    // The pseudo console keeps its own references to these handles.
    input_read.reset();
    output_write.reset();

    STARTUPINFOEXW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.StartupInfo.cb = sizeof(startup_info);

    size_t attribute_list_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_list_size);

    startup_info.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attribute_list_size));
    if (!startup_info.lpAttributeList)
    {
        PLOG(ERROR) << "HeapAlloc failed";
        return false;
    }

    const bool attributes_initialized =
        InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0, &attribute_list_size);
    const bool attributes_ready = attributes_initialized &&
        UpdateProcThreadAttribute(startup_info.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pseudo_console.get(), sizeof(void*), nullptr,
            nullptr);

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    BOOL created = FALSE;
    if (attributes_ready)
    {
        const DWORD flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;

        // Prefer PowerShell 7 (pwsh.exe), fall back to the bundled Windows PowerShell.
        const wchar_t kPwsh[] = L"pwsh.exe";
        const wchar_t kPowerShell[] = L"powershell.exe";

        created = CreateProcessW(nullptr, const_cast<wchar_t*>(kPwsh), nullptr, nullptr, FALSE, flags,
                                 nullptr, nullptr, &startup_info.StartupInfo, &process_info);
        if (!created)
        {
            created = CreateProcessW(nullptr, const_cast<wchar_t*>(kPowerShell), nullptr, nullptr,
                                     FALSE, flags, nullptr, nullptr, &startup_info.StartupInfo,
                                     &process_info);
        }
    }

    if (attributes_initialized)
        DeleteProcThreadAttributeList(startup_info.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, startup_info.lpAttributeList);

    if (!created)
    {
        PLOG(ERROR) << "Unable to start the shell process";
        return false;
    }

    ScopedHandle thread_handle(process_info.hThread);
    ScopedHandle process_handle(process_info.hProcess);

    std::error_code error_code;
    output_stream_.assign(output_read.release(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to assign output handle:" << error_code;
        return false;
    }

    pseudo_console_ = std::move(pseudo_console);
    input_write_ = std::move(input_write);
    process_ = std::move(process_handle);

    doRead();
    return true;
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessWin::writeInput(const QByteArray& data)
{
    if (!input_write_.isValid())
        return;

    DWORD written = 0;
    WriteFile(input_write_, data.constData(), static_cast<DWORD>(data.size()), &written, nullptr);
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessWin::resize(int columns, int rows)
{
    if (!pseudo_console_.isValid())
        return;

    static ResizePseudoConsoleFunc resize_pseudo_console =
        conPtyFunction<ResizePseudoConsoleFunc>("ResizePseudoConsole");
    if (!resize_pseudo_console)
        return;

    COORD size;
    size.X = static_cast<SHORT>(columns);
    size.Y = static_cast<SHORT>(rows);
    resize_pseudo_console(pseudo_console_.get(), size);
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessWin::doRead()
{
    auto guard = alive_guard_;
    output_stream_.async_read_some(asio::buffer(read_buffer_),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            emit sig_finished();
            return;
        }

        QByteArray batch(read_buffer_.data(), static_cast<int>(bytes_transferred));

        // Drain everything already buffered in the pipe before returning to the I/O context. ConPTY
        // corrupts the stream it produces whenever its output pipe is left non-empty, so the pipe is
        // emptied fully on each wakeup instead of one async read at a time.
        for (;;)
        {
            DWORD available = 0;
            if (!PeekNamedPipe(output_stream_.native_handle(), nullptr, 0, nullptr, &available,
                               nullptr) || available == 0)
            {
                break;
            }

            std::error_code read_error;
            size_t read = output_stream_.read_some(asio::buffer(read_buffer_), read_error);
            if (read_error || read == 0)
                break;

            batch.append(read_buffer_.data(), static_cast<int>(read));
        }

        emit sig_output(batch);
        doRead();
    });
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessWin::stop()
{
    // Prevent any already-queued read handler from touching this object.
    *alive_guard_ = false;

    std::error_code error_code;
    output_stream_.close(error_code);

    // Closing the pseudo console makes the shell exit.
    pseudo_console_.reset();

    if (process_.isValid())
        TerminateProcess(process_, 0);

    input_write_.reset();
    process_.reset();
}
