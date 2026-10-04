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

#ifndef HOST_CREDENTIALS_IPC_CLIENT_H
#define HOST_CREDENTIALS_IPC_CLIENT_H

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/windows/stream_handle.hpp>

class IpcClient
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called on the thread that created the client (the main UI thread).
        virtual void onCredentials(const wchar_t* domain, const wchar_t* username,
                                   const wchar_t* password) = 0;
    };

    enum class ScreenType : uint32_t
    {
        UNKNOWN = 0,
        LOGON   = 1,
        LOCK    = 2,
        UAC     = 3,
    };

    IpcClient(ScreenType screen_type, Delegate* delegate);
    ~IpcClient();

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    bool start();
    void stop();
    void postRequest();

private:
    static const uint32_t kHeaderMagic = 0x43495341; // 'A','S','I','C' read little-endian.
    static const uint32_t kFlagReliable = 1;
    static const uint32_t kFlagSecure = 2;
    static const uint32_t kRequestCredentials = 1;
    static const size_t kMaxChars = 256;

    struct Input
    {
        uint32_t magic;
        uint32_t message_size;
        uint32_t channel_id;
        uint32_t flags;
        wchar_t domain[kMaxChars];
        wchar_t username[kMaxChars];
        wchar_t password[kMaxChars];
    };

    struct Output
    {
        uint32_t magic;
        uint32_t message_size;
        uint32_t channel_id;
        uint32_t flags;
        uint32_t request_type;
        uint32_t screen_type;
    };

    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    void run();
    void tryConnect();
    void doRead();
    bool postCredentials();
    void sendRequest();
    void scheduleReconnect();
    void fatalError();

    const ScreenType screen_type_;
    Delegate* const delegate_;

    HWND window_ = nullptr;
    ATOM window_class_ = 0;

    std::wstring path_;
    Input input_ = {};
    Output output_ = {};
    bool writing_ = false;
    bool request_pending_ = false;

    asio::io_context io_context_ { 1 };
    asio::windows::stream_handle stream_;
    asio::steady_timer connect_timer_;
    WorkGuard work_guard_;
    std::thread thread_;

    std::atomic<bool> stopping_ { false };
};

#endif // HOST_CREDENTIALS_IPC_CLIENT_H
