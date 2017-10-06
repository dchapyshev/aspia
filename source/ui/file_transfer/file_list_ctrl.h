//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer/file_list_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__FILE_LIST_CTRL_H
#define _ASPIA_UI__FILE_TRANSFER__FILE_LIST_CTRL_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <memory>

namespace aspia {

class FileListCtrl : public CWindowImpl<FileListCtrl, CListViewCtrl>
{
public:
    static const int kInvalidObjectIndex = -1;
    static const int kNewFolderObjectIndex = -2;

    FileListCtrl() = default;
    virtual ~FileListCtrl() = default;

    bool CreateFileList(HWND parent, int control_id);

    void Read(std::shared_ptr<proto::FileList> list);
    void Read(const proto::DriveList& list);

    bool IsValidObjectIndex(int object_index) const;
    bool HasFileList() const;
    const proto::FileList::Item& Object(int object_index) const;
    FilePath ObjectName(int object_index) const;
    bool IsDirectoryObject(int object_index) const;
    proto::FileList::Item* FirstSelectedObject() const;
    int GetObjectUnderMousePointer() const;
    void AddDirectory();

    class Iterator
    {
    public:
        enum Mode
        {
            ALL = LVNI_ALL,
            SELECTED = LVNI_SELECTED
        };

        Iterator(const FileListCtrl& list, Mode mode);
        ~Iterator() = default;

        bool IsAtEnd() const;
        void Advance();
        const proto::FileList::Item& Object() const;

    private:
        const FileListCtrl& list_;
        const Mode mode_;
        int item_index_;
    };

private:
    BEGIN_MSG_MAP(FileListCtrl)
        // Nothing
    END_MSG_MAP()

    int GetColumnCount() const;
    void DeleteAllColumns();
    void AddNewColumn(UINT string_id, int width);

    CImageListManaged imagelist_;
    std::shared_ptr<proto::FileList> list_;

    DISALLOW_COPY_AND_ASSIGN(FileListCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__FILE_LIST_CTRL_H
