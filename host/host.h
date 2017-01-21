//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_H
#define _ASPIA_HOST__HOST_H

#include "aspia_config.h"

#include <memory>
#include <future>

#include "base/thread.h"
#include "base/macros.h"
#include "base/scoped_aligned_buffer.h"
#include "proto/proto.pb.h"
#include "network/socket_tcp.h"
#include "host/input_injector.h"
#include "host/screen_sender.h"
#include "crypto/encryptor_aes.h"
#include "crypto/decryptor_aes.h"

namespace aspia {

class Host : private Thread
{
public:
    enum class Event { Connected, Disconnected };

    typedef std::function<void(Host::Event)> OnEventCallback;

    Host(std::unique_ptr<Socket> sock, OnEventCallback on_event_callback);
    ~Host();

    bool IsDead();
    void WriteMessage(const proto::HostToClient *message);
    void ReadMessage(std::unique_ptr<proto::ClientToHost> *message);

private:
    void Worker() override;
    void OnStop() override;

    void AsyncNotify(Host::Event type);
    void DoKeyExchange();

    void ProcessMessage(const proto::ClientToHost *message);

    void ReadPointerEvent(const proto::PointerEvent &msg);
    void ReadKeyEvent(const proto::KeyEvent &msg);
    void ReadBellEvent(const proto::BellEvent &msg);
    void ReadVideoControl(const proto::VideoControl &msg);
    void ReadPowerControl(const proto::PowerControl &msg);
    void ReadClipboardControl(const proto::ClipboardControl &msg);
    void ReadClipboard(const proto::Clipboard &msg);

private:
    OnEventCallback on_event_callback_;

    std::unique_ptr<Socket> sock_;

    uint32_t feature_mask_;

    std::unique_ptr<ScreenSender> screen_sender_;
    std::unique_ptr<InputInjector> input_injector_;

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

    uint32_t write_buffer_size_;
    uint32_t read_buffer_size_;

    ScopedAlignedBuffer write_buffer_;
    ScopedAlignedBuffer read_buffer_;

    Lock write_lock_;

    DISALLOW_COPY_AND_ASSIGN(Host);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_H
