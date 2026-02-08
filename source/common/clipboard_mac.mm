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

#include "common/clipboard_mac.h"

#include "base/logging.h"

#import <Cocoa/Cocoa.h>

namespace common {

//--------------------------------------------------------------------------------------------------
ClipboardMac::ClipboardMac(QObject* parent)
    : Clipboard(parent),
      timer_(new QTimer(this))
{
    connect(timer_, &QTimer::timeout, this, &ClipboardMac::checkForChanges);
}

//--------------------------------------------------------------------------------------------------
ClipboardMac::~ClipboardMac() = default;

//--------------------------------------------------------------------------------------------------
void ClipboardMac::init()
{
    // Synchronize local change-count with the pasteboard's. The change-count is used to detect
    // clipboard changes.
    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];

    // OS X doesn't provide a clipboard-changed notification. The only way to detect clipboard
    // changes is by polling.
    startTimer();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setData(const QString& mime_type, const QByteArray& data)
{
    if (mime_type != kMimeTypeTextUtf8)
        return;

    // Write text to clipboard.
    NSString* text = QString::fromUtf8(data).toNSString();
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ text ]];

    // Update local change-count to prevent this change from being picked up by checkForChanges.
    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::startTimer()
{
    // Restart timer.
    timer_->start(std::chrono::milliseconds(1000));
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::checkForChanges()
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSInteger change_count = [pasteboard changeCount];
    if (change_count != current_change_count_)
    {
        current_change_count_ = change_count;

        NSArray* objects = [pasteboard readObjectsForClasses:@[ [NSString class] ] options:0];
        if ([objects count])
            onData(kMimeTypeTextUtf8, QString::fromNSString([objects lastObject]).toUtf8());
    }
}

} // namespace common
