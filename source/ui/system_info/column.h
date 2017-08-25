//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/column.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__COLUMN_H
#define _ASPIA_UI__SYSTEM_INFO__COLUMN_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <list>

namespace aspia {

class Column
{
public:
    Column(UINT name_id, int width)
        : name_id_(name_id),
          width_(width)
    {
        // Nothing
    }

    Column(const Column& other)
        : Column(other.name_id_, other.width_)
    {
        // Nothing
    }

    Column& operator=(const Column& other)
    {
        name_id_ = other.name_id_;
        width_ = other.width_;
        return *this;
    }

    ~Column() = default;

    CString Name() const
    {
        CString name;
        name.LoadStringW(name_id_);
        return name;
    }

    int Width() const { return width_; }

private:
    UINT name_id_;
    int width_;
};

using ColumnList = std::list<Column>;

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__COLUMN_H
