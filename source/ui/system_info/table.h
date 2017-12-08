//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/table.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__TABLE_H
#define _ASPIA_UI__SYSTEM_INFO__TABLE_H

#include "ui/system_info/group.h"
#include "ui/system_info/column_list.h"
#include "ui/system_info/row.h"

namespace aspia {

class Output;
class Category;

class Table
{
public:
    enum class Type
    {
        LIST        = 0,
        PARAM_VALUE = 1
    };

    Table(Table&& other);
    ~Table();

    static Table List(Output* output, Category* category);
    static Table ParamValue(Output* output, Category* category);

    void AddColumns(const ColumnList& column_list);

    Group AddGroup(std::string_view name);
    void AddParam(std::string_view param, const Value& value);
    Row AddRow();

    Table& operator=(Table&& other);

private:
    Table(Output* output, Category* category, Type type);

    Output* output_;

    DISALLOW_COPY_AND_ASSIGN(Table);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__TABLE_H
