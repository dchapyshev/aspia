//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/sas_injector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SAS_INJECTOR_H
#define _ASPIA_HOST__SAS_INJECTOR_H

#include "base/service.h"

namespace aspia {

static const WCHAR kSasServiceSwitch[] = L"sas-service";

class SasInjector : private Service
{
public:
    SasInjector(const std::wstring& service_id);
    ~SasInjector() = default;

    void ExecuteService();

    static void InjectSAS();

private:
    void Worker() override;
    void OnStop() override;

    DISALLOW_COPY_AND_ASSIGN(SasInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__SAS_INJECTOR_H
