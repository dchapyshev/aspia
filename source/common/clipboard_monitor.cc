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

#include "common/clipboard_monitor.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "common/clipboard_win.h"
#elif defined(Q_OS_LINUX)
#include "common/clipboard_x11.h"
#elif defined(Q_OS_MACOS)
#include "common/clipboard_mac.h"
#else
#error Not implemented
#endif

namespace common {

namespace {

#if defined(Q_OS_WINDOWS)
const base::Thread::EventDispatcher kEventDispatcher = base::Thread::QtDispatcher;
#else
const base::Thread::EventDispatcher kEventDispatcher = base::Thread::AsioDispatcher;
#endif

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::ClipboardMonitor(QObject* parent)
    : QObject(parent),
      thread_(kEventDispatcher)
{
    LOG(INFO) << "Ctor";

    connect(&thread_, &base::Thread::started, this, &ClipboardMonitor::onBeforeThreadRunning,
            Qt::DirectConnection);
    connect(&thread_, &base::Thread::finished, this, &ClipboardMonitor::onAfterThreadRunning,
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
void ClipboardMonitor::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    emit sig_injectClipboardEventPrivate(event);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::clearClipboard()
{
    emit sig_clearClipboardPrivate();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onBeforeThreadRunning()
{
    LOG(INFO) << "Thread starting";

#if defined(Q_OS_WINDOWS)
    clipboard_ = std::make_unique<common::ClipboardWin>();
#elif defined(Q_OS_LINUX)
    clipboard_ = std::make_unique<common::ClipboardX11>();
#elif defined(Q_OS_MAC)
    clipboard_ = std::make_unique<common::ClipboardMac>();
#else
#error Not implemented
#endif

    connect(clipboard_.get(), &Clipboard::sig_clipboardEvent,
            this, &ClipboardMonitor::sig_clipboardEvent,
            Qt::QueuedConnection);

    connect(this, &ClipboardMonitor::sig_injectClipboardEventPrivate,
            clipboard_.get(), &Clipboard::injectClipboardEvent,
            Qt::QueuedConnection);

    connect(this, &ClipboardMonitor::sig_clearClipboardPrivate,
            clipboard_.get(), &Clipboard::clearClipboard,
            Qt::QueuedConnection);

    clipboard_->start();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onAfterThreadRunning()
{
    LOG(INFO) << "Thread stopping";
    clipboard_.reset();
}

} // namespace common
