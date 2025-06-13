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

#include "common/clipboard.h"

#include "base/logging.h"

namespace common {

namespace {

const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

} // namespace

//--------------------------------------------------------------------------------------------------
Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Clipboard::start()
{
    init();
}

//--------------------------------------------------------------------------------------------------
void Clipboard::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (event.mime_type() == kMimeTypeTextUtf8)
    {
        // Store last injected data.
        last_data_ = QString::fromStdString(event.data());
    }
    else
    {
        LOG(LS_ERROR) << "Unsupported mime type:" << event.mime_type();
        return;
    }

    setData(last_data_);
}

//--------------------------------------------------------------------------------------------------
void Clipboard::clearClipboard()
{
    setData(QString());
}

//--------------------------------------------------------------------------------------------------
void Clipboard::onData(const QString& data)
{
    if (last_data_ == data)
        return;

    proto::desktop::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(data.toStdString());

    emit sig_clipboardEvent(event);
}

} // namespace common
