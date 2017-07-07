//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/service_manager.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_MANAGER_H
#define _ASPIA_BASE__SERVICE_MANAGER_H

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_object.h"

namespace aspia {

class ServiceManager
{
public:
    ServiceManager() = default;
    explicit ServiceManager(const std::wstring& service_short_name);
    ServiceManager(ServiceManager&& other);
    ~ServiceManager();

    //
    // Создает службу в системе и возвращает указатель на экземпляр класса
    // для управления ей.
    // Если параметр replace установлен в true и служба с указанным именем
    // уже существует в системе, то установленная служба будет остановлена
    // и удалена, а вместо нее создана новая.
    // Если параметр replace установлен в false, то при наличии службы с
    // совпадающим именем будет возвращен нулевой указатель.
    //
    static ServiceManager Create(const std::wstring& command_line,
                                 const std::wstring& service_name,
                                 const std::wstring& service_short_name,
                                 const std::wstring& service_description = std::wstring());

    static std::wstring GenerateUniqueServiceId();

    static std::wstring CreateUniqueServiceName(const std::wstring& service_name,
                                                const std::wstring& service_id);

    static bool IsServiceInstalled(const std::wstring& service_name);

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
    bool Remove();

    ServiceManager& operator=(ServiceManager&& other);

private:
    ServiceManager(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    mutable ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_MANAGER_H
