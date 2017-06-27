//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
#define _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H

#include "client/client_session.h"
#include "base/synchronization/waitable_event.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/file_manager.h"

namespace aspia {

class ClientSessionFileTransfer :
    public ClientSession,
    private UiFileManager::Delegate,
    private MessageLoopThread::Delegate
{
public:
    ClientSessionFileTransfer(const ClientConfig& config,
                              ClientSession::Delegate* delegate);
    ~ClientSessionFileTransfer();

private:
    // ClientSession implementation.
    void Send(const IOBuffer& buffer) override;

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // UiFileManager::Delegate implementation.
    void OnWindowClose() override;
    void OnDriveListRequest(UiFileManager::PanelType panel_type) override;

    void OnDirectoryListRequest(UiFileManager::PanelType panel_type,
                                const std::string& path,
                                const std::string& item) override;

    void OnCreateDirectoryRequest(UiFileManager::PanelType panel_type,
                                  const std::string& path,
                                  const std::string& name) override;

    void OnRenameRequest(UiFileManager::PanelType panel_type,
                         const std::string& path,
                         const std::string& old_name,
                         const std::string& new_name) override;

    void OnRemoveRequest(UiFileManager::PanelType panel_type,
                         const std::string& path,
                         const std::string& item_name) override;

    void OnSendFile(const std::wstring& from_path, const std::wstring& to_path) override;
    void OnRecieveFile(const std::wstring& from_path, const std::wstring& to_path) override;

    bool ReadDriveListMessage(std::unique_ptr<proto::DriveList> drive_list);
    bool ReadDirectoryListMessage(std::unique_ptr<proto::DirectoryList> directory_list);
    bool ReadFilePacketMessage(const proto::FilePacket& packet);

    std::unique_ptr<proto::file_transfer::HostToClient> SendRemoteRequest(
        const proto::file_transfer::ClientToHost& message);

    std::unique_ptr<proto::file_transfer::HostToClient> SendLocalRequest(
        const proto::file_transfer::ClientToHost& message);

    std::unique_ptr<UiFileManager> file_manager_;

    MessageLoopThread worker_thread_;
    std::shared_ptr<MessageLoopProxy> worker_;

    WaitableEvent message_reply_event_;
    std::unique_ptr<proto::file_transfer::HostToClient> message_reply_;
    std::mutex message_reply_lock_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionFileTransfer);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
