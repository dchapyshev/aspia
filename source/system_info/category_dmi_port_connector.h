//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_port_connector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__CATEGORY_DMI_PORT_CONNECTOR_H
#define _ASPIA_SYSTEM_INFO__CATEGORY_DMI_PORT_CONNECTOR_H

#include "system_info/category.h"

namespace aspia {

class CategoryDmiPortConnector : public CategoryInfo
{
public:
    CategoryDmiPortConnector();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPortConnector);
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__CATEGORY_DMI_PORT_CONNECTOR_H
