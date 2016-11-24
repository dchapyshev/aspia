/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/service_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SERVICE_WIN_H
#define _ASPIA_BASE__SERVICE_WIN_H

#include "aspia_config.h"

#include <string>

#include "base/macros.h"


class Service
{
public:
    Service(const WCHAR *service_name);
    virtual ~Service();

    //
    // Метод запускает выполнение службы и возвращает управление после ее остановки.
    // Если служба была успшено запущена и выполнена, то возвращается true, если
    // запустить выполнение службы не удалось, то возвращается false.
    //
    bool DoWork();

    //
    // Метод, в котором выполняется работа службы. После его завершения служба
    // переходит в состояние "Остановлена".
    // Метод должн быть реализован потомком класса.
    //
    virtual void Worker() = 0;

    //
    // Метод вызывается при остановке службы.
    //
    virtual void OnStop() = 0;

private:
    static void WINAPI ServiceMain(int argc, LPWSTR argv);

    static DWORD WINAPI ServiceControlHandler(DWORD control_code,
                                              DWORD event_type,
                                              LPVOID event_data,
                                              LPVOID context);

    void SetStatus(DWORD state);

private:
    std::wstring service_name_;

    SERVICE_STATUS_HANDLE status_handle_;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

#endif // _ASPIA_BASE__SERVICE_WIN_H
