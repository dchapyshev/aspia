//
// PROJECT:         Aspia
// FILE:            system_info/category.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category.h"

#include <QCoreApplication>

#include "system_info/parser/dmi_parser.h"
#include "system_info/serializer/dmi_serializer.h"
#include "system_info/ui/dmi_form.h"

namespace aspia {

#define DECLARE_CATEGORY(_name_, _icon_, _uuid_, _class_name_)                                   \
    {                                                                                            \
        _name_,                             /* Name */                                           \
        ":/icon/" ## _icon_,                /* Icon */                                           \
        _uuid_,                             /* UUID */                                           \
        _class_name_ ## Serializer::create, /* CreateSerializer */                               \
        _class_name_ ## Parser::create,     /* CreateParser */                                   \
        _class_name_ ## Form::create,       /* CreateForm */                                     \
        nullptr                             /* Child */                                          \
    }

#define DECLARE_GROUP(_name_, _icon_, _child_)                                                   \
    {                                                                                            \
        _name_,              /* Name */                                                          \
        ":/icon/" ## _icon_, /* Icon */                                                          \
        nullptr,             /* UUID */                                                          \
        nullptr,             /* CreateSerializer */                                              \
        nullptr,             /* CreateParser */                                                  \
        nullptr,             /* CreateForm */                                                    \
        _child_              /* Child */                                                         \
    }

#define DECLARE_END() { 0 }

typedef Serializer* (*CreateSerializerFunc)(QObject* parent, const QString& uuid);
typedef Parser* (*CreateParserFunc)(QObject* parent, const QString& uuid);
typedef Form* (*CreateFormFunc)(QWidget* parent, const QString& uuid);

struct CategoryList
{
    const char* name;
    const char* icon;
    const char* uuid;

    CreateSerializerFunc create_serializer;
    CreateParserFunc create_parser;
    CreateFormFunc create_form;

    const CategoryList* child;
};

namespace {

const CategoryList kHardwareList[] =
{
    DECLARE_CATEGORY("DMI", "computer.png", "{0EB15E7F-CA9C-4164-B7EA-77FEE19E545D}", Dmi),
    DECLARE_END()
};

const CategoryList kSoftwareList[] =
{
    DECLARE_END()
};

const CategoryList kNetworkList[] =
{
    DECLARE_END()
};

const CategoryList kOSList[] =
{
    DECLARE_END()
};

// This list can only contain groups.
const CategoryList kCategoryList[] =
{
    DECLARE_GROUP(QT_TRANSLATE_NOOP("CategoryName", "Hardware"),         "hardware.png", kHardwareList),
    DECLARE_GROUP(QT_TRANSLATE_NOOP("CategoryName", "Software"),         "software.png", kSoftwareList),
    DECLARE_GROUP(QT_TRANSLATE_NOOP("CategoryName", "Network"),          "network.png",  kNetworkList),
    DECLARE_GROUP(QT_TRANSLATE_NOOP("CategoryName", "Operating System"), "os.png",       kOSList),
    DECLARE_END()
};

} // namespace

//================================================================================================
// CategoryGroup implementation.
//================================================================================================

CategoryGroup::CategoryGroup(const CategoryList* current)
    : current_(current)
{
    Q_ASSERT(current_);
    Q_ASSERT(current_->name);
    Q_ASSERT(current_->icon);
    Q_ASSERT(!current_->uuid);
    Q_ASSERT(!current_->create_form);
    Q_ASSERT(current_->child);
}

CategoryGroup::CategoryGroup(const CategoryGroup& other)
    : current_(other.current_)
{
    // Nothing
}

CategoryGroup& CategoryGroup::operator=(const CategoryGroup& other)
{
    if (this == &other)
        return *this;

    current_ = other.current_;
    return *this;
}

// static
QList<CategoryGroup> CategoryGroup::rootGroups()
{
    QList<CategoryGroup> list;

    for (const CategoryList* item = kCategoryList; item->name; ++item)
    {
        if (item->child)
            list.append(CategoryGroup(item));
    }

    return list;
}

QIcon CategoryGroup::icon() const
{
    return QIcon(current_->icon);
}

QString CategoryGroup::name() const
{
    return QCoreApplication::translate("CategoryName", current_->name);
}

QList<CategoryGroup> CategoryGroup::childGroupList() const
{
    QList<CategoryGroup> list;

    for (const CategoryList* item = current_->child; item->name; ++item)
    {
        if (item->child)
            list.append(CategoryGroup(item));
    }

    return list;
}

QList<Category> CategoryGroup::childCategoryList() const
{
    QList<Category> list;

    for (const CategoryList* item = current_->child; item->name; ++item)
    {
        if (item->uuid)
            list.append(Category(item));
    }

    return list;
}

//================================================================================================
// Category implementation.
//================================================================================================

Category::Category(const CategoryList* current)
    : current_(current)
{
    Q_ASSERT(current_);
    Q_ASSERT(current_->name);
    Q_ASSERT(current_->icon);
    Q_ASSERT(current_->uuid);
    Q_ASSERT(current_->create_form);
    Q_ASSERT(!current_->child);
}

Category::Category(const Category& other)
    : current_(other.current_)
{
    // Nothing
}

Category& Category::operator=(const Category& other)
{
    if (this == &other)
        return *this;

    current_ = other.current_;
    return *this;
}

// static
QList<Category> Category::all()
{
    std::function<QList<Category>(const CategoryGroup&)> child_categories =
        [&](const CategoryGroup& group)
    {
        QList<Category> list = group.childCategoryList();

        for (const auto& child_group : group.childGroupList())
            list.append(child_categories(child_group));

        return list;
    };

    QList<Category> categories;

    for (const auto& group : CategoryGroup::rootGroups())
        categories.append(child_categories(group));

    return categories;
}

QIcon Category::icon() const
{
    return QIcon(current_->icon);
}

QString Category::name() const
{
    return QCoreApplication::translate("CategoryName", current_->name);
}

QString Category::uuid() const
{
    return current_->uuid;
}

Serializer* Category::serializer(QObject* parent) const
{
    return current_->create_serializer(parent, uuid());
}

Parser* Category::parser(QObject* parent) const
{
    return current_->create_parser(parent, uuid());
}

Form* Category::form(QWidget* parent) const
{
    return current_->create_form(parent, uuid());
}

} // namespace aspia
