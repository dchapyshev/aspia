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

#include "base/desktop/screen_capturer_win.h"

#include "base/logging.h"
#include "base/desktop/screen_capturer_dxgi.h"
#include "base/desktop/screen_capturer_gdi.h"
#include "base/desktop/screen_capturer_mirror.h"
#include "base/win/session_info.h"
#include "base/win/windows_version.h"

#include <QTimer>

namespace base {

namespace {

ScreenCapturer::ScreenType screenType(const wchar_t* desktop_name)
{
    if (_wcsicmp(desktop_name, L"winlogon") != 0)
        return ScreenCapturer::ScreenType::DESKTOP;

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
        return ScreenCapturer::ScreenType::UNKNOWN;
    }

    base::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(LS_ERROR) << "Unable to get session info";
        return ScreenCapturer::ScreenType::UNKNOWN;
    }

    if (session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE)
    {
        if (session_info.isUserLocked())
        {
            // Lock screen captured.
            return ScreenCapturer::ScreenType::LOCK;
        }
        else
        {
            // UAC screen captured.
            return ScreenCapturer::ScreenType::OTHER;
        }
    }

    return ScreenCapturer::ScreenType::LOGIN;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerWin::ScreenCapturerWin(Type type, QObject* parent)
    : ScreenCapturer(type, parent)
{
    LOG(LS_INFO) << "Ctor";

    switchToInputDesktop();

    // If the monitor is turned off, this call will turn it on.
    if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED))
    {
        PLOG(LS_ERROR) << "SetThreadExecutionState failed";
    }

    wchar_t desktop[100] = { 0 };
    if (desktop_.assignedDesktop().name(desktop, sizeof(desktop)))
    {
        if (_wcsicmp(desktop, L"Screen-saver") == 0)
        {
            auto send_key = [](WORD key_code, DWORD flags)
            {
                INPUT input;
                memset(&input, 0, sizeof(input));

                input.type       = INPUT_KEYBOARD;
                input.ki.wVk     = key_code;
                input.ki.dwFlags = flags;
                input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));

                // Do the keyboard event.
                if (!SendInput(1, &input, sizeof(input)))
                {
                    PLOG(LS_ERROR) << "SendInput failed";
                }
            };

            send_key(VK_SPACE, 0);
            send_key(VK_SPACE, KEYEVENTF_KEYUP);
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unable to get name of desktop";
    }

    QTimer::singleShot(0, this, [this]()
    {
        Desktop desktop = Desktop::inputDesktop();
        if (!desktop.isValid())
        {
            LOG(LS_ERROR) << "Unable to get input desktop";
            return;
        }

        wchar_t desktop_name[128] = { 0 };
        if (!desktop.name(desktop_name, sizeof(desktop_name)))
        {
            LOG(LS_ERROR) << "Unable to get desktop name";
            return;
        }

        checkScreenType(desktop_name);
    });
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerWin::~ScreenCapturerWin()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer* ScreenCapturerWin::create(Type preferred_type, Error last_error)
{
    auto try_mirror_capturer = []() -> ScreenCapturer*
    {
        // Mirror screen capture is available only in Windows 7/2008 R2.
        if (windowsVersion() == base::VERSION_WIN7)
        {
            LOG(LS_INFO) << "Windows 7/2008R2 detected. Try to initialize MIRROR capturer";

            std::unique_ptr<ScreenCapturerMirror> capturer_mirror =
                std::make_unique<ScreenCapturerMirror>();

            if (capturer_mirror->isSupported())
            {
                LOG(LS_INFO) << "Using MIRROR capturer";
                return capturer_mirror.release();
            }
            else
            {
                LOG(LS_INFO) << "MIRROR capturer unavailable";
            }
        }
        else
        {
            LOG(LS_INFO) << "Windows version is not equal to 7/2008R2. MIRROR capturer unavailable";
        }

        return nullptr;
    };

    ScreenCapturer* screen_capturer = nullptr;

    if (last_error == ScreenCapturer::Error::PERMANENT)
    {
        LOG(LS_INFO) << "Permanent error. Reset to GDI capturer";
    }
    else if (preferred_type == ScreenCapturer::Type::WIN_DXGI ||
             preferred_type == ScreenCapturer::Type::DEFAULT)
    {
        if (windowsVersion() >= VERSION_WIN8)
        {
            // Desktop Duplication API is available in Windows 8+.
            std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi =
                std::make_unique<ScreenCapturerDxgi>();
            if (capturer_dxgi->isSupported())
            {
                LOG(LS_INFO) << "Using DXGI capturer";
                screen_capturer = capturer_dxgi.release();
            }
        }
        else
        {
            screen_capturer = try_mirror_capturer();
        }
    }
    else if (preferred_type == ScreenCapturer::Type::WIN_MIRROR)
    {
        screen_capturer = try_mirror_capturer();
    }

    if (!screen_capturer)
    {
        LOG(LS_INFO) << "Using GDI capturer";
        screen_capturer = new ScreenCapturerGdi();
    }

    return screen_capturer;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWin::switchToInputDesktop()
{
    // Switch to the desktop receiving user input if different from the current one.
    Desktop input_desktop(Desktop::inputDesktop());

    if (!input_desktop.isValid() || desktop_.isSame(input_desktop))
        return;

    wchar_t new_name[128] = { 0 };
    input_desktop.name(new_name, sizeof(new_name));

    wchar_t old_name[128] = { 0 };
    desktop_.assignedDesktop().name(old_name, sizeof(old_name));

    LOG(LS_INFO) << "Input desktop changed from '" << old_name << "' to '" << new_name << "'";

    reset();

    // If setThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.setThreadDesktop(std::move(input_desktop));

    emit sig_desktopChanged();
    checkScreenType(new_name);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWin::checkScreenType(const wchar_t* desktop_name)
{
    ScreenType screen_type = screenType(desktop_name);
    if (screen_type != last_screen_type_)
    {
        LOG(LS_INFO) << "Screen type changed from " << screenTypeToString(last_screen_type_)
                     << " to " << screenTypeToString(screen_type);

        QString screen_name = QString::fromWCharArray(desktop_name);
        if (screen_name.isEmpty())
            screen_name = "unknown";

        emit sig_screenTypeChanged(screen_type, screen_name);
        last_screen_type_ = screen_type;
    }
    else
    {
        LOG(LS_INFO) << "Screen type not changed: " << screenTypeToString(last_screen_type_);
    }
}

} // namespace base
