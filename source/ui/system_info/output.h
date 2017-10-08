//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_H

#include "base/macros.h"
#include "ui/system_info/column.h"

#include <string>

namespace aspia {

class Output
{
public:
    virtual ~Output() = default;

    virtual void AddItem(const std::string& parameter, const std::string& value) = 0;
    virtual void AddItem(const std::string& value) = 0;

    class ScopedTable
    {
    public:
        ScopedTable(Output& output, const ColumnList& column_list)
            : output_(output)
        {
            output_.BeginTable(column_list);
        }

        ~ScopedTable()
        {
            output_.EndTable();
        }

    private:
        Output& output_;

        DISALLOW_COPY_AND_ASSIGN(ScopedTable);
    };

    class ScopedItemGroup
    {
    public:
        ScopedItemGroup(Output& output, const std::string& name)
            : output_(output)
        {
            output_.BeginItemGroup(name);
        }

        ~ScopedItemGroup()
        {
            output_.EndItemGroup();
        }

    private:
        Output& output_;

        DISALLOW_COPY_AND_ASSIGN(ScopedItemGroup);
    };

protected:
    virtual void BeginTable(const ColumnList& column_list) = 0;
    virtual void EndTable() = 0;

    virtual void BeginItemGroup(const std::string& name) = 0;
    virtual void EndItemGroup() = 0;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_H
