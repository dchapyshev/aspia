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

#include "host/credentials/ipc_client.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <new>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include "host/credentials/dll_main.h"
#include "host/credentials/logging.h"
#include "host/credentials/utils.h"

namespace {

const wchar_t kChannelName[] = L"org.aspia.host.credentials";
const UINT kCredentialsMessage = WM_APP;

struct PendingCredentials
{
    std::wstring domain;
    std::wstring username;
    std::wstring password;
};

//--------------------------------------------------------------------------------------------------
void secureClearCredentials(PendingCredentials* credentials)
{
    if (!credentials)
        return;

    secureClear(credentials->domain);
    secureClear(credentials->username);
    secureClear(credentials->password);
}

//--------------------------------------------------------------------------------------------------
bool isNullTerminated(const wchar_t* field, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (field[i] == L'\0')
            return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
bool isValidAccountString(const wchar_t* field)
{
    static const wchar_t kInvalidChars[] = L"\"/\\[]:;|=,+*?<>";

    for (const wchar_t* p = field; *p; ++p)
    {
        if (*p < 0x20 || wcschr(kInvalidChars, *p))
            return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool isConnectionAllowed(HANDLE pipe)
{
    ULONG session_id = 0;
    if (!GetNamedPipeServerSessionId(pipe, &session_id))
    {
        LOG(L"GetNamedPipeServerSessionId failed: %lu", GetLastError());
        return false;
    }

    if (session_id != 0)
    {
        LOG(L"IPC server is not in session 0 (session: %lu)", session_id);
        return false;
    }

    ULONG process_id = 0;
    if (!GetNamedPipeServerProcessId(pipe, &process_id))
    {
        LOG(L"GetNamedPipeServerProcessId failed: %lu", GetLastError());
        return false;
    }

    ScopedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!process.isValid())
    {
        LOG(L"OpenProcess failed: %lu", GetLastError());
        return false;
    }

    HANDLE token_handle = nullptr;
    if (!OpenProcessToken(process.get(), TOKEN_QUERY, &token_handle))
    {
        LOG(L"OpenProcessToken failed: %lu", GetLastError());
        return false;
    }
    ScopedHandle token(token_handle);

    BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE] = {};
    DWORD size = 0;
    if (!GetTokenInformation(token.get(), TokenUser, buffer, sizeof(buffer), &size))
    {
        LOG(L"GetTokenInformation failed: %lu", GetLastError());
        return false;
    }

    BYTE system_sid[SECURITY_MAX_SID_SIZE] = {};
    DWORD system_sid_size = sizeof(system_sid);
    if (!CreateWellKnownSid(WinLocalSystemSid, nullptr, system_sid, &system_sid_size))
    {
        LOG(L"CreateWellKnownSid failed: %lu", GetLastError());
        return false;
    }

    const TOKEN_USER* token_user = reinterpret_cast<const TOKEN_USER*>(buffer);
    return EqualSid(token_user->User.Sid, system_sid) == TRUE;
}

} // namespace

//--------------------------------------------------------------------------------------------------
IpcClient::IpcClient(ScreenType screen_type, Delegate* delegate)
    : screen_type_(screen_type),
      delegate_(delegate),
      stream_(io_context_),
      connect_timer_(io_context_),
      work_guard_(asio::make_work_guard(io_context_))
{
    LOG(L"Ctor");

    wchar_t class_name[64] = {};
    swprintf_s(class_name, L"AspiaCredentialsIpcClient.%p", static_cast<void*>(this));

    WNDCLASSEXW window_class;
    SecureZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = windowProc;
    window_class.hInstance = g_instance;
    window_class.lpszClassName = class_name;

    window_class_ = RegisterClassExW(&window_class);
    if (!window_class_)
    {
        LOG(L"RegisterClassExW failed: %lu", GetLastError());
        return;
    }

    window_ = CreateWindowExW(0, MAKEINTATOM(window_class_), nullptr, 0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, g_instance, nullptr);
    if (!window_)
    {
        LOG(L"CreateWindowExW failed: %lu", GetLastError());
        UnregisterClassW(MAKEINTATOM(window_class_), g_instance);
        window_class_ = 0;
        return;
    }

    SetLastError(0);
    SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    DWORD error = GetLastError();
    if (error != 0)
    {
        LOG(L"SetWindowLongPtrW failed: %lu", error);
        DestroyWindow(window_);
        window_ = nullptr;
        UnregisterClassW(MAKEINTATOM(window_class_), g_instance);
        window_class_ = 0;
        return;
    }
}

//--------------------------------------------------------------------------------------------------
IpcClient::~IpcClient()
{
    LOG(L"Dtor");

    stop();

    SecureZeroMemory(&input_, sizeof(input_));
    SecureZeroMemory(&output_, sizeof(output_));

    if (window_)
    {
        // The IPC thread is already joined, so no new messages can arrive. Drop any that were
        // posted but not yet dispatched, wiping the credentials they carry.
        MSG message = {};
        while (PeekMessageW(&message, window_, kCredentialsMessage, kCredentialsMessage, PM_REMOVE))
        {
            std::unique_ptr<PendingCredentials> credentials(
                reinterpret_cast<PendingCredentials*>(message.lParam));
            secureClearCredentials(credentials.get());
        }

        DestroyWindow(window_);
        window_ = nullptr;
    }

    if (window_class_)
    {
        UnregisterClassW(MAKEINTATOM(window_class_), g_instance);
        window_class_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
bool IpcClient::start()
{
    if (!window_)
    {
        LOG(L"The message window was not created");
        return false;
    }

    if (thread_.joinable())
    {
        LOG(L"Already started");
        return false;
    }

    try
    {
        path_ = L"\\\\.\\pipe\\aspia.";
        path_ += kChannelName;

        thread_ = std::thread(&IpcClient::run, this);
    }
    catch (...)
    {
        LOG(L"Failed to start the IPC thread");
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void IpcClient::stop()
{
    if (!thread_.joinable())
        return;

    LOG(L"Stopping the IPC thread");

    stopping_.store(true, std::memory_order_relaxed);
    work_guard_.reset();
    io_context_.stop();
    thread_.join();

    std::error_code ignored;
    stream_.close(ignored);

    LOG(L"IPC thread stopped");
}

//--------------------------------------------------------------------------------------------------
void IpcClient::postRequest()
{
    asio::post(io_context_, [this]() { sendRequest(); });
}

//--------------------------------------------------------------------------------------------------
// static
LRESULT CALLBACK IpcClient::windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message != kCredentialsMessage)
        return DefWindowProcW(hwnd, message, wparam, lparam);

    IpcClient* self = reinterpret_cast<IpcClient*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    std::unique_ptr<PendingCredentials> credentials(
        reinterpret_cast<PendingCredentials*>(lparam));

    if (self && credentials)
    {
        self->delegate_->onCredentials(credentials->domain.c_str(), credentials->username.c_str(),
                                       credentials->password.c_str());
    }

    secureClearCredentials(credentials.get());
    return 0;
}

//--------------------------------------------------------------------------------------------------
void IpcClient::run()
{
    try
    {
        LOG(L"IPC thread started (path: %s)", path_.c_str());

        tryConnect();
        io_context_.run();

        LOG(L"IPC thread finished");
    }
    catch (...)
    {
        LOG(L"Unhandled exception on the IPC thread");
    }
}

//--------------------------------------------------------------------------------------------------
void IpcClient::tryConnect()
{
    if (stopping_.load(std::memory_order_relaxed))
        return;

    const DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;

    ScopedHandle handle(CreateFileW(path_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, flags, nullptr));
    if (handle.isValid())
    {
        if (!isConnectionAllowed(handle.get()))
        {
            LOG(L"The IPC server is not a SYSTEM service");
            fatalError();
            return;
        }

        std::error_code error_code;
        stream_.assign(handle.get(), error_code);
        if (error_code)
        {
            LOG(L"Failed to assign handle: %d", error_code.value());
            scheduleReconnect();
            return;
        }

        handle.release(); // The stream owns the handle now.

        LOG(L"Connected");

        sendRequest();
        doRead();
        return;
    }

    const DWORD error = GetLastError();
    if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND)
        LOG(L"CreateFileW failed: %lu", error);

    scheduleReconnect();
}

//--------------------------------------------------------------------------------------------------
void IpcClient::doRead()
{
    asio::async_read(stream_, asio::buffer(&input_, sizeof(input_)),
                     [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            SecureZeroMemory(&input_, sizeof(input_));

            if (error_code != asio::error::operation_aborted)
            {
                LOG(L"Read failed: %d", error_code.value());
                scheduleReconnect();
            }
            return;
        }

        bool valid = true;

        if (bytes_transferred != sizeof(input_))
        {
            LOG(L"Short read: %lu of %lu bytes", static_cast<DWORD>(bytes_transferred),
                static_cast<DWORD>(sizeof(input_)));
            valid = false;
        }
        else if (input_.magic != kHeaderMagic)
        {
            LOG(L"Invalid header magic: 0x%08lX", input_.magic);
            valid = false;
        }
        else if (input_.message_size != sizeof(input_) - offsetof(Input, domain))
        {
            LOG(L"Unexpected message size: %lu", input_.message_size);
            valid = false;
        }

        if (valid && !postCredentials())
            valid = false;

        SecureZeroMemory(&input_, sizeof(input_));

        if (!valid)
        {
            fatalError();
            return;
        }

        if (stopping_.load(std::memory_order_relaxed))
            return;

        doRead();
    });
}

//--------------------------------------------------------------------------------------------------
bool IpcClient::postCredentials()
{
    if (!isNullTerminated(input_.domain, kMaxChars) ||
        !isNullTerminated(input_.username, kMaxChars) ||
        !isNullTerminated(input_.password, kMaxChars))
    {
        LOG(L"Malformed credentials message");
        return false;
    }

    if (!isValidAccountString(input_.domain) || !isValidAccountString(input_.username))
    {
        LOG(L"Credentials contain an invalid domain or username");
        return false;
    }

    std::unique_ptr<PendingCredentials> credentials(new(std::nothrow) PendingCredentials());
    if (!credentials)
    {
        LOG(L"PendingCredentials allocation failed");
        return true;
    }

    try
    {
        credentials->domain = input_.domain;
        credentials->username = input_.username;
        credentials->password = input_.password;
    }
    catch (...)
    {
        LOG(L"Failed to copy the credentials");
        secureClearCredentials(credentials.get());
        return true;
    }

    if (!PostMessageW(window_, kCredentialsMessage, 0, reinterpret_cast<LPARAM>(credentials.get())))
    {
        LOG(L"PostMessageW failed: %lu", GetLastError());
        secureClearCredentials(credentials.get());
        return true;
    }

    credentials.release();
    return true;
}

//--------------------------------------------------------------------------------------------------
void IpcClient::sendRequest()
{
    if (stopping_.load(std::memory_order_relaxed) || !stream_.is_open())
        return;

    if (writing_)
    {
        request_pending_ = true;
        return;
    }

    writing_ = true;

    output_.magic = kHeaderMagic;
    output_.message_size = sizeof(output_) - offsetof(Output, request_type);
    output_.channel_id = 0;
    output_.flags = kFlagReliable | kFlagSecure;
    output_.request_type = kRequestCredentials;
    output_.screen_type = static_cast<uint32_t>(screen_type_);

    asio::async_write(stream_, asio::buffer(&output_, sizeof(output_)),
                      [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        writing_ = false;

        SecureZeroMemory(&output_, sizeof(output_));

        if (error_code)
        {
            request_pending_ = false;

            if (error_code != asio::error::operation_aborted)
            {
                LOG(L"Request write failed: %d", error_code.value());
                scheduleReconnect();
            }
            return;
        }

        if (bytes_transferred != sizeof(output_))
        {
            request_pending_ = false;
            LOG(L"Short write: %lu of %lu bytes", static_cast<DWORD>(bytes_transferred),
                static_cast<DWORD>(sizeof(output_)));
            scheduleReconnect();
            return;
        }

        if (request_pending_)
        {
            request_pending_ = false;
            sendRequest();
        }
    });
}

//--------------------------------------------------------------------------------------------------
void IpcClient::scheduleReconnect()
{
    if (stopping_.load(std::memory_order_relaxed))
        return;

    std::error_code ignored;
    stream_.close(ignored);

    connect_timer_.expires_after(std::chrono::milliseconds(1000));
    connect_timer_.async_wait([this](const std::error_code& error_code)
    {
        if (error_code)
            return;

        tryConnect();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcClient::fatalError()
{
    connect_timer_.cancel();

    std::error_code ignored;
    stream_.close(ignored);

    work_guard_.reset();
}
