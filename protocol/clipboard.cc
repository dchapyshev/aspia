//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/clipboard.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/clipboard.h"
#include "base/scoped_clipboard.h"
#include "base/scoped_hglobal.h"
#include "base/unicode.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace aspia {

static const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

Clipboard::~Clipboard()
{
    Stop();
}

bool Clipboard::Start(ClipboardEventCallback clipboard_event_callback)
{
    clipboard_event_callback_ = std::move(clipboard_event_callback);

    window_.reset(new MessageWindow());

    if (!window_->Create(std::bind(&Clipboard::OnMessage,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2,
                                   std::placeholders::_3,
                                   std::placeholders::_4)))
    {
        LOG(ERROR) << "Couldn't create clipboard window.";
        return false;
    }

    HMODULE user32_module = GetModuleHandleW(L"user32.dll");
    if (user32_module)
    {
        add_clipboard_format_listener_ =
            reinterpret_cast<AddClipboardFormatListenerFn*>(
                GetProcAddress(user32_module, "AddClipboardFormatListener"));

        remove_clipboard_format_listener_ =
            reinterpret_cast<RemoveClipboardFormatListenerFn*>(
                GetProcAddress(user32_module, "RemoveClipboardFormatListener"));
    }

    if (HaveClipboardListenerApi())
    {
        if (!add_clipboard_format_listener_(window_->hwnd()))
        {
            LOG(WARNING) << "AddClipboardFormatListener() failed: "
                         << GetLastSystemErrorString();
            return false;
        }
    }
    else
    {
        next_viewer_window_ = SetClipboardViewer(window_->hwnd());
    }

    return true;
}

void Clipboard::Stop()
{
    if (window_)
    {
        if (HaveClipboardListenerApi())
        {
            remove_clipboard_format_listener_(window_->hwnd());
        }
        else
        {
            ChangeClipboardChain(window_->hwnd(), next_viewer_window_);
        }

        window_.reset();

        last_mime_type_.clear();
        last_data_.clear();

        clipboard_event_callback_ = nullptr;

        remove_clipboard_format_listener_ = nullptr;
        add_clipboard_format_listener_ = nullptr;
    }
}

bool Clipboard::HaveClipboardListenerApi()
{
    return add_clipboard_format_listener_ && remove_clipboard_format_listener_;
}

void Clipboard::OnClipboardUpdate()
{
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        std::string data;

        // Add a scope, so that we keep the clipboard open for as short a time as possible.
        {
            ScopedClipboard clipboard;

            if (!clipboard.Init(window_->hwnd()))
            {
                LOG(WARNING) << "Couldn't open the clipboard: "
                             << GetLastSystemErrorString();
                return;
            }

            HGLOBAL text_global = clipboard.GetData(CF_UNICODETEXT);
            if (!text_global)
            {
                LOG(WARNING) << "Couldn't get data from the clipboard: "
                             << GetLastSystemErrorString();
                return;
            }

            {
                ScopedHGlobal<WCHAR> text_lock(text_global);
                if (!text_lock.Get())
                {
                    LOG(WARNING) << "Couldn't lock clipboard data: "
                                 << GetLastSystemErrorString();
                    return;
                }

                if (!UNICODEtoUTF8(text_lock.Get(), data))
                {
                    LOG(WARNING) << "Couldn't convert data to utf8";
                    return;
                }
            }
        }

        if (!data.empty())
        {
            data = ReplaceCrLfByLf(data);

            if (last_mime_type_ != kMimeTypeTextUtf8 || last_data_ != data)
            {
                std::unique_ptr<proto::ClipboardEvent> event(new proto::ClipboardEvent);

                event->set_mime_type(kMimeTypeTextUtf8);
                event->set_data(data);

                clipboard_event_callback_(std::move(event));
            }
        }
    }
}

bool Clipboard::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    UNREF(wParam);
    UNREF(lParam);

    switch (message)
    {
        case WM_CLIPBOARDUPDATE:
        {
            if (!HaveClipboardListenerApi())
                return false;

            OnClipboardUpdate();
        }
        break;

        case WM_DRAWCLIPBOARD:
        {
            if (HaveClipboardListenerApi())
                return false;

            OnClipboardUpdate();
        }
        break;

        case WM_CHANGECBCHAIN:
        {
            if (HaveClipboardListenerApi())
                return false;

            // wParam - A handle to the window being removed from the clipboard viewer chain.
            // lParam - A handle to the next window in the chain following the window being removed.
            //          This parameter is NULL if the window being removed is the last window in the chain.

            if (reinterpret_cast<HWND>(wParam) == next_viewer_window_)
            {
                next_viewer_window_ = reinterpret_cast<HWND>(lParam);
            }
            else if (next_viewer_window_)
            {
                SendMessageW(next_viewer_window_, message, wParam, lParam);
            }
        }
        break;

        default:
            return false;
    }

    *result = 0;
    return true;
}

void Clipboard::InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    if (!window_ || !clipboard_event)
        return;

    // Currently we only handle UTF-8 text.
    if (clipboard_event->mime_type() != kMimeTypeTextUtf8)
    {
        LOG(WARNING) << "Unsupported mime type: " << clipboard_event->mime_type();
        return;
    }

    if (!StringIsUtf8(clipboard_event->data().c_str(), clipboard_event->data().length()))
    {
        LOG(WARNING) << "Clipboard data is not UTF-8 encoded";
        return;
    }

    // Store last injected data.
    last_mime_type_ = std::move(*clipboard_event->mutable_mime_type());
    last_data_ = std::move(*clipboard_event->mutable_data());

    std::wstring text;

    if (!UTF8toUNICODE(ReplaceLfByCrLf(last_data_), text))
    {
        LOG(WARNING) << "Couldn't convert data to unicode";
        return;
    }

    ScopedClipboard clipboard;

    if (!clipboard.Init(window_->hwnd()))
    {
        LOG(WARNING) << "Couldn't open the clipboard."
                     << GetLastSystemErrorString();
        return;
    }

    clipboard.Empty();

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(WCHAR));
    if (!text_global)
    {
        LOG(WARNING) << "GlobalAlloc() failed: " << GetLastSystemErrorString();
        return;
    }

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));

    if (!text_global_locked)
    {
        LOG(WARNING) << "GlobalLock() failed: " << GetLastSystemErrorString();
        return;
    }

    memcpy(text_global_locked, text.data(), text.size() * sizeof(WCHAR));
    text_global_locked[text.size()] = 0;

    GlobalUnlock(text_global);

    clipboard.SetData(CF_UNICODETEXT, text_global);
}

} // namespace aspia
