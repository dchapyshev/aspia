/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/message_queue.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__MESSAGE_QUEUE_H
#define _ASPIA_CLIENT__MESSAGE_QUEUE_H

#include <functional>
#include <queue>
#include <memory>

#include "base/macros.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/waitable_event.h"

namespace aspia {

template <class T>
class MessageQueue : private Thread
{
public:
    typedef std::function<void(const T*)> ProcessMessageCallback;

    MessageQueue(ProcessMessageCallback process_message) :
        process_message_(process_message)
    {
        SetThreadPriority(Thread::Priority::Highest);
        Start();
    }

    ~MessageQueue()
    {
        if (!IsThreadTerminated())
        {
            Stop();
            WaitForEnd();
        }
    }

    void MessageQueue::Add(std::unique_ptr<T> &message)
    {
        {
            // Ѕлокируем очередь сообщений.
            LockGuard<Lock> guard(&queue_lock_);

            // ƒобавл€ем сообщение в очередь.
            queue_.push(std::move(message));
        }

        // ”ведомл€ем поток о том, что в очереди есть новое сообщение.
        message_event_.Notify();
    }

private:
    void Dispatch()
    {
        //
        // ѕродолжаем обработку очереди пока не будут обработаны все сообщени€ или
        // поток не получит команду остановитьс€.
        //
        while (!queue_.empty() && !IsThreadTerminating())
        {
            std::unique_ptr<T> message;

            {
                // Ѕлокируем очередь сообщений.
                LockGuard<Lock> guard(&queue_lock_);

                // »звлекаем первое сообщение из очереди.
                message = std::move(queue_.front());

                // ”дал€ем первое сообщение из очереди.
                queue_.pop();
            }

            // ¬ызываем callback дл€ обработки сообщени€.
            process_message_(message.get());
        }
    }

    void Worker() override
    {
        while (!IsThreadTerminating())
        {
            // ќжидаем уведомлени€ о новом сообщении.
            message_event_.WaitForEvent();

            Dispatch();
        }
    }

    void OnStop() override
    {
        //
        // ≈сли потоку дана команда остановитьс€, то посылаем уведомление о
        // сообщении, чтобы прервать цикл потока.
        //
        message_event_.Notify();
    }

private:
    // Callback дл€ обработки сообщени€.
    ProcessMessageCallback process_message_;

    // ќчередь сообщений.
    std::queue <std::unique_ptr<T>> queue_;

    // Mutex дл€ блокировани€ очереди сообщений.
    Lock queue_lock_;

    //  ласс дл€ уведомлени€ о наличии новых сообщений.
    WaitableEvent message_event_;

    DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__MESSAGE_QUEUE_H
