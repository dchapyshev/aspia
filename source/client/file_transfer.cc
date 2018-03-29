//
// PROJECT:         Aspia
// FILE:            client/file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer.h"

namespace aspia {

FileTransfer::FileTransfer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

FileTransfer::Action FileTransfer::directoryCreateErrorAction() const
{
    return action_directory_create_error_;
}

void FileTransfer::setDirectoryCreateErrorAction(Action action)
{
    action_directory_create_error_ = action;
}

FileTransfer::Action FileTransfer::fileCreateErrorAction() const
{
    return action_file_create_error_;
}

void FileTransfer::setFileCreateErrorAction(Action action)
{
    action_file_create_error_ = action;
}

FileTransfer::Action FileTransfer::fileExistsAction() const
{
    return action_file_exists_;
}

void FileTransfer::setFileExistsAction(Action action)
{
    action_file_exists_ = action;
}

FileTransfer::Action FileTransfer::fileOpenErrorAction() const
{
    return action_file_open_error_;
}

void FileTransfer::setFileOpenErrorAction(Action action)
{
    action_file_open_error_ = action;
}

FileTransfer::Action FileTransfer::fileReadErrorAction() const
{
    return action_file_read_error_;
}

void FileTransfer::setFileReadErrorAction(Action action)
{
    action_file_read_error_ = action;
}

} // namespace aspia
