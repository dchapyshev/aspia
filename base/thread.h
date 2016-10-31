/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__THREAD_H
#define _ASPIA_BASE__THREAD_H

#include "aspia_config.h"

#include <stdint.h>
#include <assert.h>
#include <process.h>
#include <atomic>

#include "base/macros.h"
#include "base/mutex.h"
#include "base/logging.h"

class Thread
{
public:
    //
    // Поток изначально создается в приостановленном состоянии.
    // Для запуска потока необходимо вызвать метод Start().
    //
    Thread();
    virtual ~Thread();

    bool IsValid() const;

    //
    // Метод для инициации завершения потока.
    // При вызове данного метода автоматически вызывается реализованный
    // классом-потомком метод OnStop().
    // Метод OnStop() не вызывается, если поток не запущен или класс не
    // является валидным.
    //
    bool Stop();

    //
    // Возвращает true, если поток находится в стадии завершения и false,
    // если нет.
    //
    bool IsEndOfThread() const;

    enum class Priority
    {
        Unknown      = 0, // Неизвестный
        Idle         = 1, // Самый низкий
        Lowest       = 2, // Низкий
        BelowNormal  = 3, // Ниже нормального
        Normal       = 4, // Нормальный
        AboveNormal  = 5, // Выше нормального
        Highest      = 6, // Высокий
        TimeCritical = 7  // Наивысший
    };

    //
    // Устанавливает приоритет выполнения потока.
    //
    bool SetThreadPriority(Priority value = Priority::Normal);

    //
    // Возвращает текущий приоритет потока.
    //
    Priority GetThreadPriority() const;

    //
    // Запускает поток.
    // При вызове метода вызывается метод OnStart(), который реализуется
    // потомком класса.
    // Метод OnStart() не вызывается, если класс не является валидным или
    // если поток уже запущен.
    //
    bool Start();

    //
    // Ожидание завершения потока.
    // Если поток успешно завершился, то возвращается true, если нет, то false.
    //
    bool WaitForEnd() const;

    //
    // Ожидание завершения потока за указанное время.
    // Если поток успешно завершился, то возвращается true, если нет, то false.
    //
    bool WaitForEnd(uint32_t milliseconds) const;

    //
    // Метод вызывает засыпание потока на время, которое указано в параметре.
    //
    static void Sleep(uint32_t milliseconds);

    //
    // Возвращает уникальный идетификатор (ID) потока.
    //
    uint32_t GetThreadId() const;

    //
    // Метод, который непосредственно выполняется в потоке.
    // Метод должен быть реализован потомком класса.
    //
    virtual void Worker() = 0;

    //
    // Вызывается при вызове метода Start().
    // Метод должен быть реализован потомком класса.
    //
    virtual void OnStart() = 0;

    //
    // Вызывается при вызове метода Stop().
    // Метод должен быть реализован потомком класса.
    //
    virtual void OnStop() = 0;

private:
    static UINT CALLBACK ThreadProc(LPVOID param);

private:
    HANDLE thread_;
    uint32_t thread_id_;
    std::atomic_bool started_;

    DISALLOW_COPY_AND_ASSIGN(Thread);
};

#endif // _ASPIA_BASE__THREAD_H
