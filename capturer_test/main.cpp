/*
* PROJECT:         Aspia Remote Desktop
* FILE:            capturer_test/main.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include <direct.h>
#include "base/logging.h"
#include "capturer_test/screen_updater.h"

class Logging
{
public:
    Logging() : is_initialized_(false)
    {
        char current_path[256];

        // Получаем путь к директории, из которой запущено приложение
        if (_getcwd(current_path, _countof(current_path)))
        {
            std::string log_path(current_path);

            log_path.append("/log");

            FLAGS_log_dir      = log_path;
            FLAGS_logtostderr  = 0;
            FLAGS_minloglevel  = google::GLOG_INFO;
            FLAGS_max_log_size = 25; // 25MB

            log_path.append("/VIDEO_TEST_");

            google::SetLogDestination(google::GLOG_INFO,    log_path.c_str());
            google::SetLogDestination(google::GLOG_WARNING, log_path.c_str());
            google::SetLogDestination(google::GLOG_ERROR,   log_path.c_str());
            google::SetLogDestination(google::GLOG_FATAL,   log_path.c_str());

            google::InitGoogleLogging("aspia-remote-desktop");

            is_initialized_ = true;
        }
    }

    ~Logging()
    {
        if (is_initialized_)
        {
            google::ShutdownGoogleLogging();
        }
    }

private:
    bool is_initialized_;
};

INT WINAPI
wWinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nShowCmd)
{
    Logging logging;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    std::unique_ptr<ScreenUpdater> sender(new ScreenUpdater());

    sender->Start();
    sender->WaitForEnd();

    return 0;
}
