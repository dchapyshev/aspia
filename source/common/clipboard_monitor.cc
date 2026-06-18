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

#include "common/clipboard_monitor.h"

#include "base/logging.h"
#include "common/clipboard.h"
#include "proto/desktop_clipboard.h"

namespace {

#if defined(Q_OS_WINDOWS)
const Thread::EventDispatcher kEventDispatcher = Thread::QtDispatcher;
#else
const Thread::EventDispatcher kEventDispatcher = Thread::AsioDispatcher;
#endif

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::ClipboardMonitor(QObject* parent)
    : QObject(parent),
      thread_(kEventDispatcher)
{
    LOG(INFO) << "Ctor";

    connect(&thread_, &Thread::sig_beforeRunning, this, &ClipboardMonitor::onBeforeThreadRunning,
            Qt::DirectConnection);
    connect(&thread_, &Thread::sig_afterRunning, this, &ClipboardMonitor::onAfterThreadRunning,
            Qt::DirectConnection);
}

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::~ClipboardMonitor()
{
    LOG(INFO) << "Dtor";
    thread_.stop();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::start()
{
    LOG(INFO) << "Starting clipboard monitor";
    thread_.start();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::injectClipboardEvent(const proto::clipboard::Event& event)
{
    if (!clipboard_)
        return;

    QMetaObject::invokeMethod(
        clipboard_.get(), &Clipboard::injectClipboardEvent, Qt::QueuedConnection, event);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::clearClipboard()
{
    if (!clipboard_)
        return;

    QMetaObject::invokeMethod(clipboard_.get(), &Clipboard::clearClipboard, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::addFileData(int file_index, const QByteArray& data, bool is_last)
{
    if (!clipboard_)
        return;

    QMetaObject::invokeMethod(
        clipboard_.get(), &Clipboard::addFileData, Qt::QueuedConnection, file_index, data, is_last);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onBeforeThreadRunning()
{
    LOG(INFO) << "Thread starting";

    clipboard_.reset(Clipboard::create());

    if (!clipboard_)
    {
        LOG(INFO) << "No clipboard implementation for this platform";
        return;
    }

    connect(clipboard_.get(), &Clipboard::sig_clipboardEvent, this, &ClipboardMonitor::sig_clipboardEvent,
            Qt::QueuedConnection);
    connect(clipboard_.get(), &Clipboard::sig_fileDataRequest, this, &ClipboardMonitor::sig_fileDataRequest,
            Qt::QueuedConnection);
    connect(clipboard_.get(), &Clipboard::sig_localFileListChanged, this, &ClipboardMonitor::sig_localFileListChanged,
            Qt::QueuedConnection);

    clipboard_->start();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onAfterThreadRunning()
{
    LOG(INFO) << "Thread stopping";

    if (clipboard_)
    {
        clipboard_->clearClipboard();
        clipboard_.reset();
    }
}
