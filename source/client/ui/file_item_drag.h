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

#ifndef ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H_
#define ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H_

#include <QDrag>

#include "client/file_transfer.h"

namespace aspia {

class FileItem;
class FileItemMimeData;

class FileItemDrag : public QDrag
{
public:
    explicit FileItemDrag(QObject* drag_source = nullptr);
    virtual ~FileItemDrag() = default;

    void setFileList(const QList<FileTransfer::Item>& file_list);

private:
    DISALLOW_COPY_AND_ASSIGN(FileItemDrag);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H_
