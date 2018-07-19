//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_SYSTEM_INFO__CATEGORY_H_
#define ASPIA_SYSTEM_INFO__CATEGORY_H_

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

#endif // ASPIA_SYSTEM_INFO__CATEGORY_H_
