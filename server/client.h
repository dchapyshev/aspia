/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/client.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SERVER__CLIENT_H
#define _ASPIA_SERVER__CLIENT_H

#include "aspia_config.h"

#include <memory>
#include <future>

#include "base/thread.h"
#include "base/macros.h"
#include "base/input_handler.h"
#include "proto/auth.pb.h"
#include "proto/proto.pb.h"
#include "network/socket_tcp.h"
#include "server/screen_sender.h"

class Client : public Thread
{
public:
    typedef std::function<void(uint32_t)> OnDisconnectedCallback;

    Client(std::unique_ptr<Socket> socket,
           uint32_t client_id,
           OnDisconnectedCallback on_client_disconnected);

    ~Client();

    uint32_t GetID() const;

    void WriteMessage(const proto::ServerToClient *message);

private:
    void Worker() override;
    void OnStop() override;

    void ReadPointerEvent(const proto::PointerEvent &msg);
    void ReadKeyEvent(const proto::KeyEvent &msg);
    void ReadVideoControl(const proto::VideoControl &msg);

private:
    uint32_t client_id_;

    OnDisconnectedCallback on_client_disconnected_;

    std::unique_ptr<Socket> socket_;

    int32_t feature_mask_;

    std::unique_ptr<ScreenSender> screen_sender_;
    std::unique_ptr<InputHandler> input_handler_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

#endif // _ASPIA_SERVER__CLIENT_H
