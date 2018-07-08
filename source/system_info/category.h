//
// PROJECT:         Aspia
// FILE:            system_info/category.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__CATEGORY_H
#define _ASPIA_SYSTEM_INFO__CATEGORY_H

#include "system_info/parser/parser.h"
#include "system_info/serializer/serializer.h"
#include "system_info/ui/form.h"

namespace aspia {

struct CategoryList;
class Category;

class CategoryGroup
{
public:
    CategoryGroup(const CategoryGroup& other);
    CategoryGroup& operator=(const CategoryGroup& other);

    static QList<CategoryGroup> rootGroups();

    QIcon icon() const;
    QString name() const;
    QList<CategoryGroup> childGroupList() const;
    QList<Category> childCategoryList() const;

private:
    CategoryGroup(const CategoryList* current);
    const CategoryList* current_;
};

class Category
{
public:
    Category(const Category& other);
    Category& operator=(const Category& other);

    static QList<Category> all();

    QIcon icon() const;
    QString name() const;
    QString uuid() const;

    Serializer* serializer(QObject* parent) const;
    Parser* parser(QObject* parent) const;
    Form* form(QWidget* parent) const;

private:
    friend class CategoryGroup;
    Category(const CategoryList* current);

    const CategoryList* current_;
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__CATEGORY_H
