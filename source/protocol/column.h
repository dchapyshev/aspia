//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/column.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__COLUMN_H
#define _ASPIA_PROTOCOL__COLUMN_H

#include "base/logging.h"
#include <list>

namespace aspia {

class Column
{
public:
    Column(const std::string& name, int width)
        : name_(name),
          width_(width)
    {
        // Nothing
    }

    Column(std::string&& name, int width)
        : name_(std::move(name)),
          width_(width)
    {
        // Nothing
    }

    Column(const char* name, int width)
    {
        DCHECK(name);
        name_.assign(name);
        width_ = width;
    }

    explicit Column(const Column& other)
        : name_(other.name_),
          width_(other.width_)
    {
        // Nothing
    }

    Column& operator=(const Column& other)
    {
        name_ = other.name_;
        width_ = other.width_;
        return *this;
    }

    ~Column() = default;

    const std::string& Name() const { return name_; }
    int Width() const { return width_; }

private:
    std::string name_;
    int width_;
};

using ColumnList = std::list<Column>;

} // namespace aspia

#endif // _ASPIA_PROTOCOL__COLUMN_H
