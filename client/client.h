//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/scoped_aligned_buffer.h"
#include "network/socket_tcp.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vp8.h"
#include "codec/video_decoder_vp9.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/desktop_point.h"
#include "client/message_queue.h"
#include "client/client_config.h"
#include "crypto/encryptor_aes.h"
#include "crypto/decryptor_aes.h"

namespace aspia {

class Client : private Thread
{
public:
    enum class Event
    {
        Connected,    // Успешно подключен к серверу.
        Disconnected, // Отключен от сервера.
        NotConnected, // Не удалось подключиться.
        AuthRequest,
        BadAuth,       // Не удалось пройти авторизацию.
    };

    typedef std::function<void(Client::Event)> OnEventCallback;

    Client(std::unique_ptr<Socket> sock, OnEventCallback on_event_callback);
    ~Client();

    void SendPointerEvent(int32_t x, int32_t y, uint32_t mask);
    void SendKeyEvent(uint32_t keycode, uint32_t flags = 0);
    void SendBellEvent();
    void SendPowerControl(proto::PowerControl::Action action);
    void SendClipboardControl(uint32_t flags);
    void SendClipboard(const std::string &data);

    void DoAuthorize(const std::string &username, const std::string &password);

    typedef std::function<void(const uint8_t*, const DesktopRegion&)> OnVideoUpdateCallback;
    typedef std::function<void(const DesktopSize&, const PixelFormat&)> OnVideoResizeCallback;

    void StartScreenUpdate(const ScreenConfig &config,
                           OnVideoUpdateCallback on_update_callback,
                           OnVideoResizeCallback on_resize_callback);
    void EndScreenUpdate();
    void ApplyScreenConfig(const ScreenConfig &config);

private:
    void Worker() override;
    void OnStop() override;

    void AsyncNotify(Client::Event type);

    void DoKeyExchange();
    void WriteMessage(const proto::ClientToHost *message);
    void ReadMessage(std::unique_ptr<proto::HostToClient> *message);
    void ProcessMessage(const proto::HostToClient *message);

    void ReadVideoPacket(const proto::VideoPacket &packet);
    void ReadClipboard(const proto::Clipboard &msg);

private:
    OnEventCallback on_event_callback_;

    OnVideoUpdateCallback on_video_update_callback_;
    OnVideoResizeCallback on_video_resize_callback_;

    std::unique_ptr<Socket> sock_;
    std::unique_ptr<MessageQueue<proto::HostToClient>> input_queue_;
    std::unique_ptr<MessageQueue<proto::ClientToHost>> output_queue_;
    std::unique_ptr<VideoDecoder> decoder_;

    uint32_t encoding_;

    DesktopSize screen_size_;
    PixelFormat pixel_format_;
    DesktopRegion changed_region_;

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

    uint32_t write_buffer_size_;
    uint32_t read_buffer_size_;

    ScopedAlignedBuffer write_buffer_;
    ScopedAlignedBuffer read_buffer_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
