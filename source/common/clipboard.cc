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

#include "common/clipboard.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "common/clipboard_win.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "common/clipboard_x11.h"
#elif defined(Q_OS_MACOS)
#include "common/clipboard_mac.h"
#elif defined(Q_OS_ANDROID)
#include "common/clipboard_android.h"
#endif

namespace {

// Maximum total size of clipboard data in a single event. Formats that do not fit are dropped.
// Must stay well below TcpChannel::kMaxMessageSize (5 MB): an oversized message is a fatal
// protocol error that disconnects the whole session.
constexpr qint64 kMaxEventSize = 2 * 1024 * 1024; // 2 MB

// Mime types the implementation for the current platform can capture and inject. The referenced
// character arrays are statically initialized, so building the views here is safe.
const std::string_view kSupportedMimeTypes[] =
{
    Clipboard::kMimeTypeTextUtf8,
#if defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS) || (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID))
    Clipboard::kMimeTypeTextHtml,
    Clipboard::kMimeTypeTextRtf,
    Clipboard::kMimeTypeTextCsv,
    Clipboard::kMimeTypeImagePng,
    Clipboard::kMimeTypeImageSvg,
#elif defined(Q_OS_ANDROID)
    Clipboard::kMimeTypeTextHtml,
#endif
#if defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
    Clipboard::kMimeTypeFileList,
#endif
};

//--------------------------------------------------------------------------------------------------
// Priority of a format when the event exceeds the size limit: the least important formats are
// dropped first.
int formatPriority(const std::string& mime_type)
{
    if (mime_type == Clipboard::kMimeTypeFileList)
        return 0;
    if (mime_type == Clipboard::kMimeTypeTextUtf8)
        return 1;
    if (mime_type == Clipboard::kMimeTypeTextHtml)
        return 2;
    if (mime_type == Clipboard::kMimeTypeTextRtf)
        return 3;
    if (mime_type == Clipboard::kMimeTypeTextCsv)
        return 4;
    if (mime_type == Clipboard::kMimeTypeImagePng)
        return 5;
    if (mime_type == Clipboard::kMimeTypeImageSvg)
        return 6;
    return 7;
}

//--------------------------------------------------------------------------------------------------
bool isSupportedMimeType(const std::string& mime_type)
{
    for (std::string_view supported : kSupportedMimeTypes)
    {
        if (mime_type == supported)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// Keeps only the locally supported formats that together fit into kMaxEventSize. Duplicate mime
// types are dropped. The resulting formats are ordered by priority; the payloads are moved out of
// |event|, not copied.
proto::clipboard::Event sanitizedEvent(proto::clipboard::Event&& event)
{
    std::vector<proto::clipboard::Event::Format*> formats;
    formats.reserve(static_cast<size_t>(event.format_size()));

    for (int i = 0; i < event.format_size(); ++i)
    {
        proto::clipboard::Event::Format* format = event.mutable_format(i);
        if (isSupportedMimeType(format->mime_type()))
            formats.push_back(format);
    }

    std::stable_sort(formats.begin(), formats.end(),
        [](const proto::clipboard::Event::Format* lhs, const proto::clipboard::Event::Format* rhs)
    {
        return formatPriority(lhs->mime_type()) < formatPriority(rhs->mime_type());
    });

    proto::clipboard::Event result;
    qint64 total_size = 0;

    for (proto::clipboard::Event::Format* format : formats)
    {
        // Empty payloads carry no content, and the capture side already skips them
        // (Clipboard::addFormat), so keeping them here would make injected and captured events
        // asymmetric for no benefit.
        if (format->data().empty())
            continue;

        if (Clipboard::findFormat(result, format->mime_type()))
            continue;

        qint64 data_size = static_cast<qint64>(format->data().size());
        if (total_size + data_size > kMaxEventSize)
        {
            LOG(WARNING) << "Clipboard format" << format->mime_type() << "with size" << data_size
                         << "dropped by size limit";
            continue;
        }

        total_size += data_size;
        result.add_format()->Swap(format);
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
Clipboard* Clipboard::create(QObject* parent)
{
#if defined(Q_OS_WINDOWS)
    return new ClipboardWin(parent);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    return new ClipboardX11(parent);
#elif defined(Q_OS_MACOS)
    return new ClipboardMac(parent);
#elif defined(Q_OS_ANDROID)
    return new ClipboardAndroid(parent);
#else
    LOG(ERROR) << "No clipboard implementation for this platform";
    return nullptr;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
const proto::clipboard::Event::Format* Clipboard::findFormat(
    const proto::clipboard::Event& event, std::string_view mime_type)
{
    for (int i = 0; i < event.format_size(); ++i)
    {
        const proto::clipboard::Event::Format& format = event.format(i);
        if (format.mime_type() == mime_type)
            return &format;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
void Clipboard::addFormat(
    proto::clipboard::Event* event, std::string_view mime_type, std::string_view data)
{
    if (data.empty())
        return;

    proto::clipboard::Event::Format* format = event->add_format();
    format->set_mime_type(mime_type);
    format->set_data(data);
}

//--------------------------------------------------------------------------------------------------
// static
void Clipboard::addFormat(
    proto::clipboard::Event* event, std::string_view mime_type, const QByteArray& data)
{
    addFormat(event, mime_type,
              std::string_view(data.constData(), static_cast<size_t>(data.size())));
}

//--------------------------------------------------------------------------------------------------
const char Clipboard::kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";
const char Clipboard::kMimeTypeTextHtml[] = "text/html";
const char Clipboard::kMimeTypeTextRtf[] = "text/rtf";
const char Clipboard::kMimeTypeTextCsv[] = "text/csv";
const char Clipboard::kMimeTypeImagePng[] = "image/png";
const char Clipboard::kMimeTypeImageSvg[] = "image/svg+xml";
const char Clipboard::kMimeTypeFileList[] = "file-list";

//--------------------------------------------------------------------------------------------------
void Clipboard::start()
{
    init();
}

//--------------------------------------------------------------------------------------------------
void Clipboard::injectClipboardEvent(const proto::clipboard::Event& event)
{
    proto::clipboard::Event sanitized = sanitizedEvent(proto::clipboard::Event(event));
    if (!sanitized.format_size())
    {
        LOG(ERROR) << "No supported formats in clipboard event";
        return;
    }

    setData(sanitized);
}

//--------------------------------------------------------------------------------------------------
void Clipboard::clearClipboard()
{
    setData(proto::clipboard::Event());
}

//--------------------------------------------------------------------------------------------------
void Clipboard::addFileData(int /* file_index */, const QByteArray& /* data */, bool /* is_last */)
{
    // Default implementation does nothing. Overridden by implementations with file transfer
    // support (ClipboardWin, ClipboardMac).
}

//--------------------------------------------------------------------------------------------------
void Clipboard::onData(proto::clipboard::Event&& event)
{
    proto::clipboard::Event sanitized = sanitizedEvent(std::move(event));
    if (!sanitized.format_size())
        return;

    // The change caused by our own injection is suppressed per platform (owner check on Windows,
    // change count on macOS, selection ownership on X11, injected-value compare on Android).
    emit sig_clipboardEvent(sanitized);
}
