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
#include <QString>
#include <QTimer>

#include "base/logging.h"
#include "base/linux/x11_headers.h"

namespace {

// Canonical mime types whose X target name matches the mime type itself. Text is special-cased:
// it travels as the UTF8_STRING/STRING targets.
const char* const kMimeTargets[] =
{
    Clipboard::kMimeTypeTextHtml,
    Clipboard::kMimeTypeTextRtf,
    Clipboard::kMimeTypeTextCsv,
    Clipboard::kMimeTypeImagePng,
    Clipboard::kMimeTypeImageSvg
};

} // namespace

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

    std::vector<std::string> targets;
    targets.emplace_back("UTF8_STRING");
    targets.emplace_back("STRING");
    for (const char* target : kMimeTargets)
        targets.emplace_back(target);

    x_server_clipboard_ = std::make_unique<XServerClipboard>();
    x_server_clipboard_->init(display_, targets,
        std::bind(&ClipboardX11::onClipboardChanged, this, std::placeholders::_1));

    x_connection_notifier_ = new QSocketNotifier(
        ConnectionNumber(display_), QSocketNotifier::Read, this);
    connect(x_connection_notifier_, &QSocketNotifier::activated, this, [this]()
    {
        pumpXEvents();
    });

    tick_timer_ = new QTimer(this);
    connect(tick_timer_, &QTimer::timeout, this, [this]()
    {
        // Drain events that a blocking round-trip (e.g. setClipboard) may have left in Xlib's queue
        // without leaving the socket readable, so the notifier alone would never deliver them.
        pumpXEvents();
        x_server_clipboard_->onTick();
    });
    tick_timer_->start(1000);

    pumpXEvents();
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::setData(const proto::clipboard::Event& event)
{
    if (!x_server_clipboard_)
        return;

    // An empty event clears the clipboard: take the selections over with no data to serve.
    if (!event.format_size())
    {
        x_server_clipboard_->setClipboard({});
        pumpXEvents();
        return;
    }

    XServerClipboard::FormatMap formats;

    const proto::clipboard::Event::Format* text_format = findFormat(event, kMimeTypeTextUtf8);
    if (text_format)
        formats["UTF8_STRING"] = text_format->data();

    for (const char* mime_type : kMimeTargets)
    {
        const proto::clipboard::Event::Format* format = findFormat(event, mime_type);
        if (format)
            formats[mime_type] = format->data();
    }

    if (formats.empty())
        return;

    x_server_clipboard_->setClipboard(std::move(formats));

    // setClipboard() does XGetSelectionOwner round-trips that can pull events into Xlib's queue
    // without leaving the socket readable; drain them now so the notifier does not miss them.
    pumpXEvents();
}

//--------------------------------------------------------------------------------------------------
void ClipboardX11::onClipboardChanged(XServerClipboard::FormatMap&& formats)
{
    proto::clipboard::Event event;

    auto text_it = formats.find("UTF8_STRING");
    if (text_it == formats.end())
    {
        // A STRING payload is Latin-1 per ICCCM, but many owners store UTF-8 bytes there.
        // Recode only when the payload is not valid UTF-8, otherwise recoding would double-encode
        // the text into mojibake.
        text_it = formats.find("STRING");
        if (text_it != formats.end())
        {
            const QByteArrayView view(text_it->second.data(),
                                      static_cast<qsizetype>(text_it->second.size()));
            if (!view.isValidUtf8())
            {
                text_it->second =
                    QString::fromLatin1(text_it->second.data(),
                                        static_cast<qsizetype>(text_it->second.size()))
                        .toUtf8().toStdString();
            }
        }
    }
    if (text_it != formats.end() && !text_it->second.empty())
    {
        proto::clipboard::Event::Format* format = event.add_format();
        format->set_mime_type(kMimeTypeTextUtf8);
        format->set_data(std::move(text_it->second));
    }

    for (const char* mime_type : kMimeTargets)
    {
        auto it = formats.find(mime_type);
        if (it == formats.end() || it->second.empty())
            continue;

        proto::clipboard::Event::Format* format = event.add_format();
        format->set_mime_type(mime_type);
        format->set_data(std::move(it->second));
    }

    onData(std::move(event));
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
