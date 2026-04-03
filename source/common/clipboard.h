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

#ifndef COMMON_CLIPBOARD_H
#define COMMON_CLIPBOARD_H

#include <QObject>

#include "proto/desktop_clipboard.h"

namespace common {

class Clipboard : public QObject
{
    Q_OBJECT

public:
    explicit Clipboard(QObject* parent);
    virtual ~Clipboard() = default;

    static const QString kMimeTypeTextUtf8;
    static const QString kMimeTypeFileList;

public slots:
    void start();
    void injectClipboardEvent(const proto::clipboard::Event& event);
    void clearClipboard();

    virtual void addFileData(int file_index, const QByteArray& data, bool is_last);

signals:
    void sig_clipboardEvent(const proto::clipboard::Event& event);
    void sig_fileDataRequest(int file_index);
    void sig_localFileListChanged(const proto::clipboard::Event::FileList& files);

protected:
    virtual void init() = 0;
    virtual void setData(const QString& mime_type, const QByteArray& data) = 0;
    void onData(const QString& mime_type, const QByteArray& data);

private:
    QByteArray last_data_;
};

} // namespace common

#endif // COMMON_CLIPBOARD_H
