//
// PROJECT:         Aspia
// FILE:            system_info/category.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__CATEGORY_H
#define _ASPIA_SYSTEM_INFO__CATEGORY_H

#include "report/table.h"

#include <list>
#include <map>
#include <memory>

namespace aspia {

class Category;
class CategoryGroup;
class CategoryInfo;

using CategoryList = std::list<std::unique_ptr<Category>>;
using CategoryMap = std::map<std::string, std::unique_ptr<CategoryInfo>>;

class Category
{
public:
    enum class Type { GROUP, INFO_LIST, INFO_PARAM_VALUE };
    using IconId = int;

    virtual ~Category() = default;

    virtual IconId Icon() const = 0;
    virtual const char* Name() const = 0;

    Type type() const { return type_; }
    CategoryGroup* category_group();
    CategoryInfo* category_info();

protected:
    Category(Type type);

private:
    const Type type_;
};

class CategoryGroup : public Category
{
public:
    virtual ~CategoryGroup() = default;

    const CategoryList& child_list() const;
    CategoryList* mutable_child_list();

protected:
    CategoryGroup();

private:
    CategoryList child_list_;
};

class CategoryInfo : public Category
{
public:
    virtual ~CategoryInfo() = default;

    virtual const char* Guid() const = 0;
    virtual void Parse(Table& table, const std::string& data) = 0;
    virtual std::string Serialize() = 0;

    bool IsChecked() const { return checked_; }
    void SetChecked(bool value) { checked_ = value; }

protected:
    CategoryInfo(Type type);

private:
    bool checked_ = false;
};

CategoryList CreateCategoryTree();
CategoryMap CreateCategoryMap();

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__CATEGORY_H
