//
// SmartCafe Project
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

#include "common/clipboard_x11.h"

#include "base/logging.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/x11/x_server_clipboard.h"

namespace common {

//--------------------------------------------------------------------------------------------------
ClipboardX11::ClipboardX11(QObject* parent)
    : Clipboard(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ClipboardX11::~ClipboardX11()
{
    if (display_)
    {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::init()
{
    display_ = XOpenDisplay(nullptr);
    if (!display_)
    {
        LOG(ERROR) << "Couldn't open X display";
        return;
    }

    x_server_clipboard_ = std::make_unique<base::XServerClipboard>();
    x_server_clipboard_->init(
        display_, std::bind(&ClipboardX11::onTextData, this, std::placeholders::_1));

    x_connection_watcher_ = std::make_unique<base::FileDescriptorWatcher>();
    x_connection_watcher_->startWatching(
        ConnectionNumber(display_),
        base::FileDescriptorWatcher::Mode::WATCH_READ,
        std::bind(&ClipboardX11::pumpXEvents, this));

    pumpXEvents();
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::setData(const QString& mime_type, const QByteArray& data)
{
    if (mime_type != kMimeTypeTextUtf8)
        return;

    if (x_server_clipboard_)
        x_server_clipboard_->setClipboard(data.toStdString());
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::onTextData(const std::string& text)
{
    onData(kMimeTypeTextUtf8, QByteArray::fromStdString(text));
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::pumpXEvents()
{
    DCHECK(display_ && x_server_clipboard_);

    while (XPending(display_))
    {
        XEvent event;
        XNextEvent(display_, &event);
        x_server_clipboard_->processXEvent(&event);
    }
}

} // namespace common
