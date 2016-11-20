/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include <atlbase.h>
#include <atlapp.h>

#include <gflags/gflags.h>
#include "base/service_control_win.h"
#include "base/runas_service.h"
#include "base/sas_injector.h"
#include "base/process.h"
#include "gui/main_dialog.h"

DEFINE_string(run_mode, "", "Run Mode");

void StartGUI()
{
    HINSTANCE instance = nullptr;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<WCHAR*>(&StartGUI),
                       &instance);
    CHECK(instance) << "GetModuleHandleExW() failed: " << GetLastError();

    HRESULT res = CoInitialize(nullptr);
    CHECK(SUCCEEDED(res)) << "CoInitialize() failed: " << res;

    AtlInitCommonControls(ICC_BAR_CLASSES);

    CAppModule module;
    res = module.Init(nullptr, instance);
    CHECK(SUCCEEDED(res)) << "Module initialization failure: " << res;

    CMainDialog main_dialog;
    main_dialog.DoModal();

    module.Term();

    CoUninitialize();
}

int main(int argc, char *argv[])
{
    // Парсим параметры коммандной строки.
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // Инициализируем библиотеку логгирования.
    google::InitGoogleLogging(argv[0]);

    bool have_admin_rights = Process::IsHaveAdminRights();

    // Если процесс запущен без прав администратора.
    if (!have_admin_rights)
    {
        // Пытаемся запустить процесс через UAC
        if (!Process::Elevate(L""))
        {
            // Если запустить не удалось, то запускаем с текущими правами.
            StartGUI();
        }
    }
    // Если администраторские права есть и установлен флаг запуска службы.
    else if (have_admin_rights && FLAGS_run_mode == "sas")
    {
        SasInjector().DoService();
    }
    // Если администраторские права есть и установлен флаг запуска службы.
    else if (have_admin_rights && FLAGS_run_mode == "runas")
    {
        RunAsService().DoService();
    }
    // Если администраторские права имеются и установлен флаг запуска от имени системы.
    else if (have_admin_rights && FLAGS_run_mode == "system")
    {
        // Запускаем GUI.
        StartGUI();
    }
    else if (have_admin_rights && FLAGS_run_mode == "service")
    {
        // TODO: Режим работы в виде службы.
    }
    // Если администраторские права есть, но никакие флаги запуска не установлены.
    else if (have_admin_rights)
    {
        // Если служба не была успешно установлена.
        if (!RunAsService::InstallAndStartService())
        {
            // Запускаем GUI.
            StartGUI();
        }
    }
    else
    {
        // Во всех остальных случаях запускаем GUI.
        StartGUI();
    }

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
