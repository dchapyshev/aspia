//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/group.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__GROUP_H
#define _ASPIA_UI__SYSTEM_INFO__GROUP_H

#include "ui/system_info/value.h"

namespace aspia {

class Output;

class Group
{
public:
    Group(Group&& other);
    ~Group();

    Group AddGroup(std::string_view name);
    void AddParam(std::string_view param, const Value& value);

    Group& operator=(Group&& other);

private:
    friend class Table;

    Group(Output* output, std::string_view name);

    Output* output_;

    DISALLOW_COPY_AND_ASSIGN(Group);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__GROUP_H
