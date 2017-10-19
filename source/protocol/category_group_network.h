//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_network.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H

#include "protocol/category_group.h"

namespace aspia {

class CategoryGroupNetwork : public CategoryGroup
{
public:
    CategoryGroupNetwork();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupNetwork);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H
