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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_MIME_DATA_H
#define CLIENT_UI_FILE_TRANSFER_FILE_MIME_DATA_H

#include "client/file_transfer.h"

#include <QMimeData>

namespace client {

class FileListModel;

class FileMimeData final : public QMimeData
{
public:
    FileMimeData() = default;
    virtual ~FileMimeData() final;

    static QString createMimeType();

    void setMimeType(const QString& mime_type);
    QString mimeType() const { return mime_type_; }

    void setFileList(const QList<FileTransfer::Item>& file_list);
    QList<FileTransfer::Item> fileList() const { return file_list_; }

    void setSource(const FileListModel* source);
    const FileListModel* source() const { return source_; }

private:
    const FileListModel* source_;
    QString mime_type_;
    QList<FileTransfer::Item> file_list_;

    DISALLOW_COPY_AND_ASSIGN(FileMimeData);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_MIME_DATA_H
