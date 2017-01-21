//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__THREAD_H
#define _ASPIA_BASE__THREAD_H

#include "aspia_config.h"

#include <stdint.h>
#include <atomic>

#include "base/scoped_handle.h"
#include "base/macros.h"
#include "base/lock.h"

namespace aspia {

class Thread
{
public:
    //
    // Поток изначально создается в приостановленном состоянии.
    // Для запуска потока необходимо вызвать метод Start().
    //
    Thread();
    virtual ~Thread();

    // Запускает поток.
    void Start();

    //
    // Метод для инициации завершения потока.
    // При вызове данного метода автоматически вызывается реализованный
    // классом-потомком метод OnStop().
    //
    void Stop();

    //
    // Возвращает true, если поток находится в стадии завершения и false,
    // если нет.
    //
    bool IsThreadTerminating() const;

    //
    // Возаращает true, если поток выполняется (выполняется метод Worker())
    // и false, если не выполняется.
    //
    bool IsActiveThread() const;

    enum class Priority
    {
        Idle,        // Самый низкий
        Lowest,      // Низкий
        BelowNormal, // Ниже нормального
        Normal,      // Нормальный
        AboveNormal, // Выше нормального
        Highest,     // Высокий
        TimeCritical // Наивысший
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

protected:
    //
    // Метод, который непосредственно выполняется в потоке.
    // Метод должен быть реализован потомком класса.
    //
    virtual void Worker() = 0;

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

} // namespace aspia

#endif // _ASPIA_BASE__THREAD_H
