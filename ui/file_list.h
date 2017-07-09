//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_list.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_LIST_H
#define _ASPIA_UI__FILE_LIST_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <memory>

namespace aspia {

class UiFileList : public CWindowImpl<UiFileList, CListViewCtrl>
{
public:
    static const int kInvalidObjectIndex = -1;
    static const int kNewFolderObjectIndex = -2;

    UiFileList() = default;
    virtual ~UiFileList() = default;

    bool CreateFileList(HWND parent, int control_id);

    void Read(std::unique_ptr<proto::FileList> list);
    void Read(const proto::DriveList& list);

    bool IsValidObjectIndex(int object_index) const;
    bool HasDirectoryList() const;
    const proto::FileList::Item& Object(int object_index);
    FilePath ObjectName(int object_index);
    bool IsDirectoryObject(int object_index);
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

        Iterator(const UiFileList& list, Mode mode);
        ~Iterator() = default;

        bool IsAtEnd() const;
        void Advance();
        proto::FileList::Item* Object() const;

    private:
        const UiFileList& list_;
        const Mode mode_;
        int item_index_;
    };

private:
    BEGIN_MSG_MAP(UiFileList)
        // Nothing
    END_MSG_MAP()

    int GetColumnCount() const;
    void DeleteAllColumns();
    void AddNewColumn(UINT string_id, int width);

    CImageListManaged imagelist_;
    std::unique_ptr<proto::FileList> list_;

    DISALLOW_COPY_AND_ASSIGN(UiFileList);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_LIST_H
