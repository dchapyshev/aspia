//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/clipboard.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/clipboard.h"

#include "base/scoped_clipboard.h"
#include "base/scoped_hglobal.h"
#include "base/unicode.h"
#include "base/logging.h"
#include "base/util.h"

namespace aspia {

static const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

Clipboard::Clipboard()
{
    // Nothing
}

Clipboard::~Clipboard()
{
    DestroyMessageWindow();
}

void Clipboard::StartExchange(const ClipboardEventCallback& clipboard_event_callback)
{
    clipboard_event_callback_ = clipboard_event_callback;
    CreateMessageWindow();
}

void Clipboard::StopExchange()
{
    DestroyMessageWindow();
}

void Clipboard::OnClipboardUpdate()
{
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        std::string data;

        // Add a scope, so that we keep the clipboard open for as short a time as possible.
        {
            ScopedClipboard clipboard;

            if (!clipboard.Init(GetMessageWindowHandle()))
            {
                LOG(WARNING) << "Couldn't open the clipboard: " << GetLastError();
                return;
            }

            HGLOBAL text_global = clipboard.GetData(CF_UNICODETEXT);
            if (!text_global)
            {
                LOG(WARNING) << "Couldn't get data from the clipboard: " << GetLastError();
                return;
            }

            {
                ScopedHGlobal<WCHAR> text_lock(text_global);
                if (!text_lock.Get())
                {
                    LOG(WARNING) << "Couldn't lock clipboard data: " << GetLastError();
                    return;
                }

                data = UTF8fromUNICODE(text_lock.Get());
            }
        }

        if (!data.empty())
        {
            data = ReplaceCrLfByLf(data);

            if (last_mime_type_ != kMimeTypeTextUtf8 || last_data_ != data)
            {
                proto::ClipboardEvent event;

                event.set_mime_type(kMimeTypeTextUtf8);
                event.set_data(data);

                clipboard_event_callback_(event);
            }
        }
    }
}

void Clipboard::OnMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            if (!AddClipboardFormatListener(hwnd))
            {
                LOG(WARNING) << "AddClipboardFormatListener() failed: "
                             << GetLastError();
            }
        }
        break;

        case WM_DESTROY:
        {
            RemoveClipboardFormatListener(hwnd);
        }
        break;

        case WM_CLIPBOARDUPDATE:
        {
            OnClipboardUpdate();
        }
        break;
    }
}

void Clipboard::InjectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!GetMessageWindowHandle())
        return;

    // Currently we only handle UTF-8 text.
    if (event.mime_type() != kMimeTypeTextUtf8)
    {
        LOG(WARNING) << "Unsupported mime type: " << event.mime_type();
        return;
    }

    // Store last injected data.
    last_mime_type_ = event.mime_type();
    last_data_ = event.data();

    std::wstring text = UNICODEfromUTF8(ReplaceLfByCrLf(event.data()));

    ScopedClipboard clipboard;

    if (!clipboard.Init(GetMessageWindowHandle()))
    {
        LOG(WARNING) << "Couldn't open the clipboard." << GetLastError();
        return;
    }

    clipboard.Empty();

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(WCHAR));
    if (!text_global)
    {
        LOG(WARNING) << "GlobalAlloc() failed: " << GetLastError();
        return;
    }

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));

    if (!text_global_locked)
    {
        LOG(WARNING) << "GlobalLock() failed: " << GetLastError();
        return;
    }

    memcpy(text_global_locked, text.data(), text.size() * sizeof(WCHAR));
    text_global_locked[text.size()] = 0;

    GlobalUnlock(text_global);

    clipboard.SetData(CF_UNICODETEXT, text_global);
}

} // namespace aspia
