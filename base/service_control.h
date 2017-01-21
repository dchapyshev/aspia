//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/service_control.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_CONTROL_H
#define _ASPIA_BASE__SERVICE_CONTROL_H

#include "aspia_config.h"

#include <memory>

#include "base/macros.h"
#include "base/scoped_sc_handle.h"

namespace aspia {

class ServiceControl
{
public:
    explicit ServiceControl(const WCHAR *service_short_name);
    ~ServiceControl();

    //
    // Создает службу в системе и возвращает указатель на экземпляр класса
    // для управления ей.
    // Если параметр replace установлен в true и служба с указанным именем
    // уже существует в системе, то установленная служба будет остановлена
    // и удалена, а вместо нее создана новая.
    // Если параметр replace установлен в false, то при наличии службы с
    // совпадающим именем будет возвращен нулевой указатель.
    //
    static std::unique_ptr<ServiceControl> Install(const WCHAR *exec_path,
                                                   const WCHAR *service_name,
                                                   const WCHAR *service_short_name,
                                                   const WCHAR *service_description,
                                                   bool replace = true);

    //
    // Проверяет валидность экземпляра класса.
    // Если экземпляр класса валидный, то возвращается true, если нет, то false.
    //
    bool IsValid() const;

    //
    // Запускает службу.
    // Если служба успешно запущена, то возвращается true, если нет, то false.
    //
    bool Start() const;

    //
    // Останавливает службу.
    // Если служба успешно остановлена, то возвращается true, если нет, то false.
    //
    bool Stop() const;

    //
    // Удаляет службу из системы.
    // После вызова метода класс становится невалидным, вызовы других методов
    // будут возвращаться с ошибкой и метод IsValid() возвратит false.
    //
    bool Delete();

private:
    ServiceControl(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    mutable ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceControl);
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_CONTROL_H
