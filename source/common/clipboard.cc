//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/clipboard.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/win/message_window.h"
#include "base/win/scoped_clipboard.h"
#include "base/win/scoped_hglobal.h"
#include "build/build_config.h"

namespace common {

namespace {

const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

} // namespace

Clipboard::Clipboard() = default;

Clipboard::~Clipboard()
{
    stop();
}

void Clipboard::start(Delegate* delegate)
{
    if (window_)
        return;

    delegate_ = delegate;
    DCHECK(delegate_);

    window_ = std::make_unique<base::win::MessageWindow>();

    if (!window_->create(std::bind(&Clipboard::onMessage,
                                   this,
                                   std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(LS_ERROR) << "Couldn't create clipboard window.";
        return;
    }

    if (!AddClipboardFormatListener(window_->hwnd()))
    {
        PLOG(LS_ERROR) << "AddClipboardFormatListener failed";
        return;
    }
}

void Clipboard::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!window_)
        return;

    // Currently we only handle UTF-8 text.
    if (event.mime_type() != kMimeTypeTextUtf8)
    {
        LOG(LS_WARNING) << "Unsupported mime type: " << event.mime_type();
        return;
    }

    // Store last injected data.
    last_mime_type_ = event.mime_type();
    last_data_ = event.data();

    std::wstring text;

    if (!base::utf8ToWide(base::replaceLfByCrLf(last_data_), &text))
    {
        LOG(LS_WARNING) << "Couldn't convert data to unicode";
        return;
    }

    base::win::ScopedClipboard clipboard;

    if (!clipboard.init(window_->hwnd()))
    {
        PLOG(LS_WARNING) << "Couldn't open the clipboard";
        return;
    }

    clipboard.empty();

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!text_global)
    {
        PLOG(LS_WARNING) << "GlobalAlloc failed";
        return;
    }

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));
    if (!text_global_locked)
    {
        PLOG(LS_WARNING) << "GlobalLock failed";
        GlobalFree(text_global);
        return;
    }

    memcpy(text_global_locked, text.data(), text.size() * sizeof(wchar_t));
    text_global_locked[text.size()] = 0;

    GlobalUnlock(text_global);

    clipboard.setData(CF_UNICODETEXT, text_global);
}

void Clipboard::stop()
{
    if (!window_)
        return;

    RemoveClipboardFormatListener(window_->hwnd());
    window_.reset();

    last_mime_type_.clear();
    last_data_.clear();

    delegate_ = nullptr;
}

bool Clipboard::onMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    switch (message)
    {
        case WM_CLIPBOARDUPDATE:
            onClipboardUpdate();
            break;

        default:
            return false;
    }

    result = 0;
    return true;
}

void Clipboard::onClipboardUpdate()
{
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
        return;

    std::string data;

    // Add a scope, so that we keep the clipboard open for as short a time as possible.
    {
        base::win::ScopedClipboard clipboard;

        if (!clipboard.init(window_->hwnd()))
        {
            PLOG(LS_WARNING) << "Couldn't open the clipboard";
            return;
        }

        HGLOBAL text_global = clipboard.data(CF_UNICODETEXT);
        if (!text_global)
        {
            PLOG(LS_WARNING) << "Couldn't get data from the clipboard";
            return;
        }

        {
            base::win::ScopedHGLOBAL<wchar_t> text_lock(text_global);
            if (!text_lock.get())
            {
                PLOG(LS_WARNING) << "Couldn't lock clipboard data";
                return;
            }

            if (!base::wideToUtf8(text_lock.get(), &data))
            {
                LOG(LS_WARNING) << "Couldn't convert data to utf8";
                return;
            }
        }
    }

    if (!data.empty())
    {
        data = base::replaceCrLfByLf(data);

        if (last_mime_type_ != kMimeTypeTextUtf8 || last_data_ != data)
        {
            proto::ClipboardEvent event;

            event.set_mime_type(kMimeTypeTextUtf8);
            event.set_data(data);

            delegate_->onClipboardEvent(event);
        }
    }
}

} // namespace common
