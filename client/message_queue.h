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
#include "base/mutex.h"
#include "base/event.h"

template <class T>
class MessageQueue : public Thread
{
public:
    typedef std::function<void(const T*)> ProcessMessageCallback;

    MessageQueue(ProcessMessageCallback process_message) :
        process_message_(process_message)
    {
        // Nothing
    }

    ~MessageQueue()
    {
        // Nothing
    }

    void MessageQueue::Add(std::unique_ptr<T> &message)
    {
        {
            // Ѕлокируем очередь сообщений.
            LockGuard<Mutex> guard(&queue_lock_);

            // ƒобавл€ем сообщение в очередь.
            queue_.push(std::move(message));
        }

        // ”ведомл€ем поток о том, что в очереди есть новое сообщение.
        message_event_.Notify();
    }

private:
    void Worker() override
    {
        while (true)
        {
            // ќжидаем уведомлени€ о новом сообщении.
            message_event_.WaitForEvent();

            // ≈сли потоку дана компанда остановитьс€, прекращаем цикл.
            if (IsEndOfThread()) break;

            // ѕродолжаем обработку очереди пока не будут обработаны все сообщени€.
            while (queue_.size())
            {
                std::unique_ptr<T> message;

                {
                    // Ѕлокируем очередь сообщений.
                    LockGuard<Mutex> guard(&queue_lock_);

                    // »звлекаем первое сообщение из очереди.
                    message = std::move(queue_.front());

                    // ”дал€ем первое сообщение из очереди.
                    queue_.pop();
                }

                // ¬ызываем callback дл€ обработки сообщени€.
                process_message_(message.get());
            }
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
    Mutex queue_lock_;

    //  ласс дл€ уведомлени€ о наличии новых сообщений.
    Event message_event_;

    DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

#endif // _ASPIA_CLIENT__MESSAGE_QUEUE_H
