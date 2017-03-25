//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_server.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_server.h"
#include "host/desktop_session_launcher.h"
#include "base/scoped_privilege.h"

namespace aspia {

static const uint32_t kAttachTimeout = 30000; // 30 seconds

DesktopSessionServer::DesktopSessionServer() :
    state_(State::Detached)
{
    // Nothing
}

void DesktopSessionServer::StartSession(const OutgoingMessageCallback& outgoing_message_callback,
                                        const ErrorCallback& error_callback)
{
    outgoing_message_callback_ = outgoing_message_callback;
    error_callback_ = error_callback;

    timer_.Start(kAttachTimeout, error_callback_);

    // Запускаем отслеживание изменений сессии.
    StartSessionWatching();
}

void DesktopSessionServer::StopSession()
{
    StopSessionWatching();

    // Останавливаем таймер.
    timer_.Stop();
}

void DesktopSessionServer::OnSessionAttached(uint32_t session_id)
{
    DCHECK(state_ != State::Detached); // Сессия должна быть отключена.
    DCHECK(!process_.IsValid());       // Процесс должен быть закрыт.
    DCHECK(!ipc_channel_);             // Межпроцессный канал должен быть отключен.

    std::wstring input_channel_id;
    std::wstring output_channel_id;

    // Создаем канал для обмена данными с хост-процессом сессии.
    std::unique_ptr<PipeServerChannel> ipc_channel(PipeServerChannel::Create(&input_channel_id,
                                                                             &output_channel_id));
    if (!ipc_channel)
        return;

    // Запускаем новую сессию.
    if (!DesktopSessionLauncher::LaunchSession(session_id, input_channel_id, output_channel_id))
        return;

    // Подключаемся к новой сессии.
    if (!ipc_channel->Connect(outgoing_message_callback_))
        return;

    {
        //
        // Для открытия хост-процесса и его завершения требуются дополнительные привилегии.
        // Если текущий процесс выполняется из службы, то данные привилегии уже имеются,
        // если нет, то включаем их.
        //
        ScopedProcessPrivilege privilege;
        privilege.Enable(SE_DEBUG_NAME);

        // Открываем хост-процесс.
        process_ = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, ipc_channel->GetPeerPid());
    }

    if (!process_.IsValid())
    {
        LOG(ERROR) << "Unable to open session process: " << GetLastError();
        return;
    }

    ObjectWatcher::SignalCallback host_process_close_callback =
        std::bind(&DesktopSessionServer::OnHostProcessClose, this);

    if (!process_watcher_.StartWatching(process_.Handle(), host_process_close_callback))
        return;

    ipc_channel_ = std::move(ipc_channel);

    state_ = State::Attached;

    // Сессия подключена, останавливаем таймер.
    timer_.Stop();
}

void DesktopSessionServer::OnSessionDetached()
{
    //
    // Мы имеем два потока, которые отслеживают завершение сессии и завершение хост-процесса.
    // Сессия может быть завершена любым из них. Если сессия уже отключена (или находится в
    // стадии завершения), то выходим.
    //
    if (state_ == State::Detached)
        return;

    state_ = State::Detached;

    process_watcher_.StopWatching();

    {
        ScopedProcessPrivilege privilege;
        privilege.Enable(SE_DEBUG_NAME);

        process_.Terminate(0);
    }

    process_.Close();

    ipc_channel_.reset();

    //
    // Запускаем таймер. Если новая сессия не будет подключена в течение
    // указанного интервала времени, то произошла ошибка.
    //
    timer_.Start(kAttachTimeout, error_callback_);
}

void DesktopSessionServer::OnHostProcessClose()
{
    switch (state_)
    {
        case State::Attached:
            // TODO: Попытка перезапуска хост-процесса.
            DetachSession();
            break;

        case State::Detached:
            break;
    }
}

void DesktopSessionServer::OnMessageFromClient(const uint8_t* buffer, uint32_t size)
{
    if (ipc_channel_)
        ipc_channel_->WriteMessage(buffer, size);
}

} // namespace aspia
