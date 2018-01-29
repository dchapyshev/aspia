//
// PROJECT:         Aspia
// FILE:            ui/address_book/computer_list_ctrl.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK__COMPUTER_LIST_CTRL_H
#define _ASPIA_UI__ADDRESS_BOOK__COMPUTER_LIST_CTRL_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

#include "proto/address_book.pb.h"

namespace aspia {

class ComputerListCtrl : public CWindowImpl<ComputerListCtrl, CListViewCtrl>
{
public:
    ComputerListCtrl() = default;

    bool Create(HWND parent, UINT control_id);

    int AddComputer(proto::Computer* computer);
    void AddChildComputers(proto::ComputerGroup* computer_group);
    proto::Computer* GetComputer(int item_index);
    void UpdateComputer(int item_index, proto::Computer* computer);

private:
    BEGIN_MSG_MAP(ComputerListCtrl)
        // Nothing
    END_MSG_MAP()

    CImageListManaged imagelist_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK__COMPUTER_LIST_CTRL_H
