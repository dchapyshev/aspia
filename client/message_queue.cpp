/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/message_queue.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/message_queue.h"

MessageQueue::MessageQueue(ProcessMessageCallback &process_message) :
    process_message_(process_message),
    message_event_("message_event")
{
    // Nothing
}

MessageQueue::~MessageQueue()
{
    // Nothing
}

void MessageQueue::Add(std::unique_ptr<proto::ServerToClient> message)
{
    // Ѕлокируем очередь сообщений.
    LockGuard<Mutex> guard(&queue_lock_);

    // ƒобавл€ем сообщение в очередь.
    queue_.push(std::move(message));

    // ”ведомл€ем поток о том, что в очереди есть новое сообщение.
    message_event_.Notify();
}

void MessageQueue::Worker()
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
            std::unique_ptr<proto::ServerToClient> message;

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

void MessageQueue::OnStart()
{
    // Nothing
}

void MessageQueue::OnStop()
{
    //
    // ≈сли потоку дана команда остановитьс€, то посылаем уведомление о
    // сообщении, чтобы прервать цикл потока.
    //
    message_event_.Notify();
}
