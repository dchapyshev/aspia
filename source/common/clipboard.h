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

#include <QMetaType>
#include <QObject>
#include <QVector>

#include <string_view>

#include "proto/desktop_clipboard.h"

struct LocalFileEntry
{
    QString path;
    bool is_dir = false;
    qint64 file_size = 0;
    qint64 create_time = 0;
    qint64 access_time = 0;
    qint64 modify_time = 0;
};

Q_DECLARE_METATYPE(LocalFileEntry)

class Clipboard : public QObject
{
    Q_OBJECT

public:
    explicit Clipboard(QObject* parent);
    virtual ~Clipboard() = default;

    // Creates the clipboard implementation for the current platform (nullptr if unsupported).
    static Clipboard* create(QObject* parent = nullptr);

    // Returns the format with the given mime type or nullptr if the event does not contain it.
    static const proto::clipboard::Event::Format* findFormat(
        const proto::clipboard::Event& event, std::string_view mime_type);

    // Appends a format to |event|; empty data is skipped.
    static void addFormat(
        proto::clipboard::Event* event, std::string_view mime_type, std::string_view data);
    static void addFormat(
        proto::clipboard::Event* event, std::string_view mime_type, const QByteArray& data);

    static const char kMimeTypeTextUtf8[];
    static const char kMimeTypeTextHtml[];
    static const char kMimeTypeTextRtf[];
    static const char kMimeTypeTextCsv[];
    static const char kMimeTypeImagePng[];
    static const char kMimeTypeImageSvg[];
    static const char kMimeTypeFileList[];

public slots:
    void start();
    void injectClipboardEvent(const proto::clipboard::Event& event);
    void clearClipboard();

    virtual void addFileData(int file_index, const QByteArray& data, bool is_last);

signals:
    void sig_clipboardEvent(const proto::clipboard::Event& event);
    void sig_fileDataRequest(int file_index);
    void sig_localFileListChanged(const QVector<LocalFileEntry>& files);

protected:
    virtual void init() = 0;
    virtual void setData(const proto::clipboard::Event& event) = 0;
    void onData(proto::clipboard::Event&& event);
};

#endif // COMMON_CLIPBOARD_H
