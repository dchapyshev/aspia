/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/sas_injector.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_HOST__SAS_INJECTOR_H
#define _ASPIA_HOST__SAS_INJECTOR_H

#include <memory>

#include "base/scoped_kernel32_library.h"
#include "base/service.h"
#include "base/thread.h"
#include "host/scoped_sas_police.h"

namespace aspia {

//
// Класс можно использовать только начиная с Windows Vista.
//
class SasInjector : private Service
{
public:
    SasInjector();
    ~SasInjector();

    void DoService();

    void InjectSAS();

private:
    void Worker() override;
    void OnStop() override;

    void SendSAS();

private:
    std::unique_ptr<ScopedKernel32Library> kernel32_;

    DISALLOW_COPY_AND_ASSIGN(SasInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__SAS_INJECTOR_H
