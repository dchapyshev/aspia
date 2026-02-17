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

namespace common {

//--------------------------------------------------------------------------------------------------
Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
const QString Clipboard::kMimeTypeTextUtf8 = QStringLiteral("text/plain; charset=UTF-8");
const QString Clipboard::kMimeTypeFileList = QStringLiteral("file-list");

//--------------------------------------------------------------------------------------------------
void Clipboard::start()
{
    init();
}

//--------------------------------------------------------------------------------------------------
void Clipboard::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
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

    proto::desktop::ClipboardEvent event;
    event.set_mime_type(mime_type.toStdString());
    event.set_data(data.toStdString());

    emit sig_clipboardEvent(event);
}

} // namespace common
