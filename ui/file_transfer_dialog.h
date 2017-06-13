//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER_DIALOG_H

#include "ui/base/modal_dialog.h"

namespace aspia {

class FileTransferDialog : public ModalDialog
{
public:
    FileTransferDialog() = default;

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER_DIALOG_H
