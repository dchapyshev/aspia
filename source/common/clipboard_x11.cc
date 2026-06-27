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

#include "common/clipboard_x11.h"

#include <QSocketNotifier>

#include "base/logging.h"
#include "base/linux/libx11.h"
#include "base/linux/x_server_clipboard.h"

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
        LibX11::closeDisplay(display_);
        display_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::init()
{
    display_ = LibX11::openDisplay(nullptr);
    if (!display_)
    {
        LOG(ERROR) << "Couldn't open X display";
        return;
    }

    x_server_clipboard_ = std::make_unique<XServerClipboard>();
    x_server_clipboard_->init(
        display_, std::bind(&ClipboardX11::onTextData, this, std::placeholders::_1));

    x_connection_notifier_ = new QSocketNotifier(
        ConnectionNumber(display_), QSocketNotifier::Read, this);
    connect(x_connection_notifier_, &QSocketNotifier::activated, this, [this]()
    {
        pumpXEvents();
    });

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

    while (LibX11::pending(display_))
    {
        XEvent event;
        LibX11::nextEvent(display_, &event);
        x_server_clipboard_->processXEvent(&event);
    }
}
