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

#include "base/logging.h"
#include "proto/desktop_clipboard.h"

#if defined(Q_OS_WINDOWS)
#include "common/clipboard_win.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "common/clipboard_x11.h"
#elif defined(Q_OS_MACOS)
#include "common/clipboard_mac.h"
#elif defined(Q_OS_ANDROID)
#include "common/clipboard_android.h"
#endif

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
const QString Clipboard::kMimeTypeTextUtf8 = "text/plain; charset=UTF-8";
const QString Clipboard::kMimeTypeFileList = "file-list";

//--------------------------------------------------------------------------------------------------
void Clipboard::start()
{
    init();
}

//--------------------------------------------------------------------------------------------------
void Clipboard::injectClipboardEvent(const proto::clipboard::Event& event)
{
    QString mime_type = QString::fromStdString(event.mime_type());

    if (mime_type == kMimeTypeTextUtf8 || mime_type == kMimeTypeFileList)
    {
        // Store last injected data.
        last_data_ = QByteArray::fromStdString(event.data());
    }
    else
    {
        LOG(ERROR) << "Unsupported mime type:" << event.mime_type();
        return;
    }

    setData(mime_type, last_data_);
}

//--------------------------------------------------------------------------------------------------
void Clipboard::clearClipboard()
{
    setData(QString(), QByteArray());
}

//--------------------------------------------------------------------------------------------------
void Clipboard::onData(const QString& mime_type, const QByteArray& data)
{
    if (last_data_ == data)
        return;

    proto::clipboard::Event event;
    event.set_mime_type(mime_type.toStdString());
    event.set_data(data.toStdString());

    emit sig_clipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void Clipboard::addFileData(int /* file_index */, const QByteArray& /* data */, bool /* is_last */)
{
    // Default implementation does nothing. Overridden in ClipboardWin.
}
