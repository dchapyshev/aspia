//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_H

#include "base/macros.h"
#include "base/logging.h"
#include "ui/system_info/column.h"

#include <memory>

namespace aspia {

class Category;
using CategoryList = std::list<std::unique_ptr<Category>>;

class Category
{
public:
    enum class Type { GROUP, REGULAR };

    virtual ~Category() = default;

    Type type() const { return type_; }

    CIcon Icon() const
    {
        return AtlLoadIconImage(icon_id_,
                                LR_CREATEDIBSECTION,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON));
    }

    CString Name() const
    {
        CString name;
        name.LoadStringW(name_id_);
        return name;
    }

    const ColumnList& column_list() const { return column_list_; }
    ColumnList* mutable_column_list() { return &column_list_; }

    const CategoryList& child_list() const
    {
        DCHECK(type_ == Type::GROUP);
        return child_list_;
    }

    CategoryList* mutable_child_list()
    {
        DCHECK(type_ == Type::GROUP);
        return &child_list_;
    }

protected:
    Category(Type type, UINT name_id, UINT icon_id)
        : type_(type),
          name_id_(name_id),
          icon_id_(icon_id)
    {
        // Nothing
    }

private:
    const Type type_;
    const UINT name_id_;
    const UINT icon_id_;

    ColumnList column_list_;
    CategoryList child_list_;

    DISALLOW_COPY_AND_ASSIGN(Category);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_H
