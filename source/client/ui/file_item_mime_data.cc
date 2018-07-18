//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_item_mime_data.h"

namespace aspia {

// static
QString FileItemMimeData::mimeType()
{
    return QStringLiteral("application/file_list");
}

void FileItemMimeData::setFileList(const QList<FileTransfer::Item>& file_list)
{
    file_list_ = file_list;
    setData(mimeType(), QByteArray());
}

} // namespace aspia
