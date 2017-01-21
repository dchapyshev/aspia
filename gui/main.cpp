//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/main.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include <atlbase.h>
#include <atlapp.h>

#include <gflags/gflags.h>
#include "base/service_control.h"
#include "base/runas_service.h"
#include "base/process.h"
#include "host/sas_injector.h"
#include "gui/main_dialog.h"

namespace aspia {

CAppModule _module;
CIcon _small_icon;
CIcon _big_icon;

DEFINE_string(run_mode, "", "Run Mode");

static void StartGUI()
{
    Process::Current().SetPriority(Process::Priority::High);

    HRESULT res = CoInitialize(nullptr);
    CHECK(SUCCEEDED(res)) << "CoInitialize() failed: " << res;

    AtlInitCommonControls(ICC_BAR_CLASSES);

    res = _module.Init(nullptr, GetModuleHandleW(nullptr));
    CHECK(SUCCEEDED(res)) << "Module initialization failure: " << res;

    _small_icon = AtlLoadIconImage(IDI_MAINICON,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));

    _big_icon = AtlLoadIconImage(IDI_MAINICON,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));

    MainDialog main_dialog;
    main_dialog.DoModal();

    _module.Term();
    CoUninitialize();
}

} // namespace aspia

using namespace aspia;

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
    // Если администраторские права есть.
    else
    {
        // Если установлен флаг запуска службы.
        if (FLAGS_run_mode == "sas")
        {
            SasInjector().DoService();
        }
        // Если установлен флаг запуска службы.
        else if (FLAGS_run_mode == "runas")
        {
            RunAsService().DoService();
        }
        // Если установлен флаг запуска от имени системы.
        else if (FLAGS_run_mode == "system")
        {
            // Запускаем GUI.
            StartGUI();
        }
        else if (FLAGS_run_mode == "service")
        {
            // TODO: Режим работы в виде службы.
        }
        // Если никакие флаги запуска не установлены.
        else
        {
            // Если служба не была успешно установлена.
            if (!RunAsService::InstallAndStartService())
            {
                // Запускаем GUI.
                StartGUI();
            }
        }
    }

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
