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

#include "client/ui/file_item_drag.h"

#include "client/ui/file_item_mime_data.h"

namespace aspia {

FileItemDrag::FileItemDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

void FileItemDrag::setFileList(const QList<FileTransfer::Item>& file_list)
{
    FileItemMimeData* mime_data = new FileItemMimeData();
    mime_data->setFileList(file_list);
    setMimeData(mime_data);
}

} // namespace aspia
