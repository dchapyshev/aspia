//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_list.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_LIST_H
#define _ASPIA_UI__FILE_LIST_H

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
    static const int kNewFolderObjectIndex = -1;

    UiFileList() = default;
    virtual ~UiFileList() = default;

    void Read(std::unique_ptr<proto::DirectoryList> list);
    void Read(const proto::DriveList& list);

    bool IsValidObjectIndex(int object_index) const;
    bool HasDirectoryList() const;
    bool HasParentDirectory() const;
    const std::string& CurrentPath() const;
    const proto::DirectoryListItem& Object(int object_index);
    const std::string& ObjectName(int object_index);
    proto::DirectoryListItem* FirstSelectedObject() const;
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
        proto::DirectoryListItem* Object() const;

    private:
        const UiFileList& list_;
        const Mode mode_;
        int item_index_;
    };

private:
    BEGIN_MSG_MAP(UiFileList)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    int GetColumnCount() const;
    void DeleteAllColumns();
    void AddNewColumn(UINT string_id, int width);

    CImageListManaged imagelist_;
    std::unique_ptr<proto::DirectoryList> list_;

    DISALLOW_COPY_AND_ASSIGN(UiFileList);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_LIST_H
