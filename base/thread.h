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
#include <atomic>

#include "base/scoped_handle.h"
#include "base/macros.h"
#include "base/mutex.h"

class Thread
{
public:
    //
    // Поток изначально создается в приостановленном состоянии.
    // Для запуска потока необходимо вызвать метод Start().
    //
    Thread();
    virtual ~Thread();

    //
    // Запускает поток.
    // При вызове метода вызывается метод OnStart(), который реализуется
    // потомком класса.
    // Метод OnStart() не вызывается, если класс не является валидным или
    // если поток уже запущен.
    //
    void Start();

    //
    // Метод для инициации завершения потока.
    // При вызове данного метода автоматически вызывается реализованный
    // классом-потомком метод OnStop().
    // Метод OnStop() не вызывается, если поток не запущен или класс не
    // является валидным.
    //
    void Stop();

    //
    // Возвращает true, если поток находится в стадии завершения и false,
    // если нет.
    //
    bool IsEndOfThread() const;

    enum class Priority
    {
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
    void SetThreadPriority(Priority value = Priority::Normal);

    //
    // Ожидание завершения потока.
    // Если поток успешно завершился, то возвращается true, если нет, то false.
    //
    void WaitForEnd() const;

    //
    // Ожидание завершения потока за указанное время.
    // Если поток успешно завершился, то возвращается true, если нет, то false.
    //
    void WaitForEnd(uint32_t milliseconds) const;

    //
    // Метод вызывает засыпание потока на время, которое указано в параметре.
    //
    static void Sleep(uint32_t milliseconds);

    //
    // Возвращает уникальный идетификатор (ID) потока.
    //
    uint32_t GetThreadId() const;

protected:
    //
    // Метод, который непосредственно выполняется в потоке.
    // Метод должен быть реализован потомком класса.
    //
    virtual void Worker() = 0;

    //
    // Вызывается при вызове метода Start() до непосредственного
    // запуска потока. Метод вызывается из того же потока, что и
    // метод Start().
    // Метод должен быть реализован потомком класса.
    //
    virtual void OnStart() = 0;

    //
    // Вызывается при вызове метода Stop() до непосредственной
    // остановки потока. Метод вызывается из того же потока, что и
    // метод Stop().
    // Метод должен быть реализован потомком класса.
    //
    virtual void OnStop() = 0;

private:
    static UINT CALLBACK ThreadProc(LPVOID param);

private:
    // Дискриптор потока.
    mutable ScopedHandle thread_;

    //
    // Значение true говорит о том, что поток выполняется
    // (выполняется метод Worker(), при завершении которого
    // флаг принимает значение false).
    //
    std::atomic_bool active_;

    //
    // Значение true говорит о том, что потоку дана команда завершиться.
    // При вызове метода Stop() флаг принимает значение false.
    //
    std::atomic_bool end_;

    DISALLOW_COPY_AND_ASSIGN(Thread);
};

#endif // _ASPIA_BASE__THREAD_H
