//
// PROJECT:         Aspia
// FILE:            ui/system_info/row.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__ROW_H
#define _ASPIA_UI__SYSTEM_INFO__ROW_H

#include "ui/system_info/value.h"

namespace aspia {

class Output;
class Table;
class Group;

class Row
{
public:
    Row(Row&& other);
    ~Row();

    void AddValue(const Value& value);

    Row& operator=(Row&& other);

private:
    friend class Table;
    friend class Group;

    Row(Output* output);

    Output* output_;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__ROW_H
