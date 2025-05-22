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

#include "client/ui/file_transfer/file_mime_data.h"

#include <QUuid>

namespace client {

//--------------------------------------------------------------------------------------------------
FileMimeData::~FileMimeData() = default;

//--------------------------------------------------------------------------------------------------
// static
QString FileMimeData::createMimeType()
{
    return QString("application/%1").arg(QUuid::createUuid().toString());
}

//--------------------------------------------------------------------------------------------------
void FileMimeData::setMimeType(const QString& mime_type)
{
    mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
void FileMimeData::setFileList(const QList<FileTransfer::Item>& file_list)
{
    file_list_ = file_list;
    setData(mimeType(), QByteArray());
}

//--------------------------------------------------------------------------------------------------
void FileMimeData::setSource(const FileListModel* source)
{
    source_ = source;
}

} // namespace client
