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

#ifndef COMMON_CLIPBOARD_MONITOR_H
#define COMMON_CLIPBOARD_MONITOR_H

#include "base/thread.h"
#include "common/clipboard.h"

namespace common {

class ClipboardMonitor final : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(QObject* parent = nullptr);
    ~ClipboardMonitor() final;

    void start();

    void injectClipboardEvent(const proto::clipboard::Event& event);
    void clearClipboard();
    void addFileData(int file_index, const QByteArray& data, bool is_last);

signals:
    void sig_clipboardEvent(const proto::clipboard::Event& event);
    void sig_fileDataRequest(int file_index);
    void sig_localFileListChanged(const QVector<common::LocalFileEntry>& files);

    void sig_injectClipboardEventPrivate(const proto::clipboard::Event& event);
    void sig_clearClipboardPrivate();
    void sig_addFileDataPrivate(int file_index, const QByteArray& data, bool is_last);

private slots:
    void onBeforeThreadRunning();
    void onAfterThreadRunning();

private:
    base::Thread thread_;
    std::unique_ptr<common::Clipboard> clipboard_;

    Q_DISABLE_COPY_MOVE(ClipboardMonitor)
};

} // namespace common

#endif // COMMON_CLIPBOARD_MONITOR_H
