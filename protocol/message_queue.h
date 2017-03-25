//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/message_queue.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__MESSAGE_QUEUE_H
#define _ASPIA_PROTOCOL__MESSAGE_QUEUE_H

#include <functional>
#include <queue>
#include <memory>

#include "base/macros.h"
#include "base/thread.h"
#include "base/condition_variable.h"

namespace aspia {

template <class T>
class MessageQueue : private Thread
{
public:
    typedef std::function<bool(const T*)> ProcessMessageCallback;

    explicit MessageQueue(const ProcessMessageCallback& process_message_callback) :
        process_message_callback_(process_message_callback),
        event_(&queue_lock_)
    {
        Start(Thread::Priority::AboveNormal);
    }

    ~MessageQueue()
    {
        Stop();
        Wait();
    }

    void MessageQueue::Add(std::unique_ptr<T>&& message)
    {
        {
            // Блокируем очередь сообщений.
            AutoLock lock(queue_lock_);

            // Добавляем сообщение в очередь.
            queue_.push(std::move(message));
        }

        // Уведомляем поток о том, что в очереди есть новое сообщение.
        event_.Signal();
    }

private:
    void Worker() override
    {
        while (true)
        {
            std::unique_ptr<T> message;

            //
            // Область видимости блокировки для возможности добавления сообщений в
            // очередь во время обработки сообщения.
            //
            {
                // Блокируем очередь сообщений.
                AutoLock lock(queue_lock_);

                //
                // Условные переменные могут неожиданно просыпаться (даже если не были
                // вызваны соответствующие методы/функции). После просыпания условной
                // переменной повторно проверяем наличии сообщений в очереди. Если
                // сообщений нет, то снова засыпаем.
                //
                while (queue_.empty())
                {
                    //
                    // Ожидаем уведомления о новом сообщении, во время ожидания
                    // метод разблокирует очередь сообщений.
                    //
                    event_.Wait();

                    // Если поток находится в стадии завершения, то выходим.
                    if (IsTerminating())
                        return;
                }

                // Извлекаем первое сообщение из очереди.
                message = std::move(queue_.front());

                // Удаляем первое сообщение из очереди.
                queue_.pop();
            }

            // Вызываем callback для обработки сообщения.
            if (!process_message_callback_(message.get()))
                return;
        }
    }

    void OnStop() override
    {
        //
        // Если потоку дана команда остановиться, то посылаем уведомление о
        // сообщении, чтобы прервать цикл потока.
        //
        event_.Signal();
    }

private:
    // Callback дл¤ обработки сообщения.
    ProcessMessageCallback process_message_callback_;

    // Очередь сообщений.
    std::queue<std::unique_ptr<T>> queue_;

    // Mutex дл¤ блокирования очереди сообщений.
    Lock queue_lock_;

    //  Класс для уведомления о наличии новых сообщений.
    ConditionVariable event_;

    DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__MESSAGE_QUEUE_H
