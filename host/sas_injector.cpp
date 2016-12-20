/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/sas_injector.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "host/sas_injector.h"

#include "base/service_control.h"

namespace aspia {

static const WCHAR kSasServiceName[] = L"aspia-sas-service";

SasInjector::SasInjector() :
    Service(kSasServiceName),
    kernel32_(new ScopedKernel32Library())
{
    // Nothing
}

SasInjector::~SasInjector()
{
    // Nothing
}

//
// Данный метод вызова SendSAS работает только если мы вызываем из службы.
// Делать реализацию отправки от имени непривилегированного пользователя
// смысла нет, т.к. если нет прав пользователя SYSTEM, мы все равно не
// сможем переключиться на другой рабочий стол после вызова.
//
void SasInjector::SendSAS()
{
    std::unique_ptr<ScopedSasPolice> sas_police(new ScopedSasPolice());

    DWORD session_id = kernel32_->WTSGetActiveConsoleSessionId();

    if (session_id == 0xFFFFFFFF)
    {
        DLOG(ERROR) << "Wrong session id";
        return;
    }

    ScopedNativeLibrary wmsgapi("wmsgapi.dll");

    typedef DWORD(WINAPI *PWMSGSENDMESSAGE)(DWORD session_id, UINT msg, WPARAM wParam, LPARAM lParam);

    PWMSGSENDMESSAGE send_message_func =
        reinterpret_cast<PWMSGSENDMESSAGE>(wmsgapi.GetFunctionPointer("WmsgSendMessage"));

    if (!send_message_func)
    {
        LOG(ERROR) << "WmsgSendMessage() not found in wmsgapi.dll";
        return;
    }

    BOOL as_user_ = FALSE;
    send_message_func(session_id, 0x208, 0, reinterpret_cast<LPARAM>(&as_user_));
}

void SasInjector::InjectSAS()
{
    DWORD session_id = 0;

    // Получаем ID сессии пользователя под которым запущен текущий процесс.
    if (!kernel32_->ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        DLOG(WARNING) << "ProcessIdToSessionId() failed: " << GetLastError();
        return;
    }

    //
    // Если получен ID сессии, то приложение запущено не как служба, а для
    // выполнения SAS нам необходимо выполнить код из службы.
    //
    if (session_id)
    {
        WCHAR module_path[MAX_PATH];

        // Получаем полный путь к исполняемому файлу.
        if (!GetModuleFileNameW(nullptr, module_path, ARRAYSIZE(module_path)))
        {
            LOG(ERROR) << "GetModuleFileNameW() failed: " << GetLastError();
            return;
        }

        std::wstring command_line(module_path);

        // Добавляем флаг запуска в виде службы.
        command_line += L" --run_mode=sas";

        // Устанавливаем службу в системе.
        std::unique_ptr<ServiceControl> service_control =
            ServiceControl::Install(command_line.c_str(),
                                    kSasServiceName,
                                    kSasServiceName,
                                    kSasServiceName,
                                    true);

        // Если служба установлена.
        if (service_control)
        {
            // Запускаем ее.
            service_control->Start();
        }
    }
    else
    {
        SendSAS();
    }
}

void SasInjector::DoService()
{
    // Запускаем службу для выполнения метода Worker().
    DoWork();

    // Удаляем службу.
    ServiceControl(kSasServiceName).Delete();
}

void SasInjector::Worker()
{
    SendSAS();
}

void SasInjector::OnStop()
{
    // Nothing
}

} // namespace aspia
