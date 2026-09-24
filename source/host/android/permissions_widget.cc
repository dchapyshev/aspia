//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/android/permissions_widget.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJniEnvironment>
#include <QJniObject>
#include <QVariant>
#include <QVBoxLayout>

#include "base/logging.h"
#include "common/android/button.h"
#include "common/android/card.h"
#include "common/android/label.h"
#include "common/android/scroll_area.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kCardSpacing = 16;

const char kInputServiceClass[] = "org/aspia/host/InputService";
const char kMenuClass[] = "org/aspia/host/FloatingMenu";
const char kActivityClass[] = "org/aspia/host/HostActivity";

//--------------------------------------------------------------------------------------------------
// Opens "All files access" of the app in the settings of the system.
void openStorageSettings(const QJniObject& context)
{
    QJniObject package_name = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + package_name.toString()).object<jstring>());

    QJniObject intent("android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
        QJniObject::fromString("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION")
            .object<jstring>(),
        uri.object());

    const jint flag_new_task = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", flag_new_task);

    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
}

} // namespace

//--------------------------------------------------------------------------------------------------
PermissionsWidget::PermissionsWidget(QWidget* parent)
    : QWidget(parent)
{
    LOG(INFO) << "Ctor";

    ScrollArea* scroll_area = new ScrollArea(this);
    QWidget* content = new QWidget(scroll_area);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kCardSpacing);

    Label* intro = new Label(
        tr("The following permissions are required for the app to work. Grant them in the system "
           "settings."),
        Label::Role::BODY, content);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    const struct
    {
        Permission permission;
        QString name;
    } permissions[] =
    {
        // The names the settings of Android use, so the user finds them there.
        { Permission::ACCESSIBILITY, tr("Accessibility service") },
        { Permission::OVERLAY, tr("Display over other apps") },
        { Permission::STORAGE, tr("All files access") },
        { Permission::NOTIFICATIONS, tr("Notifications") }
    };

    for (const auto& item : permissions)
    {
        Card* card = new Card(content);

        Label* name = new Label(item.name, Label::Role::BODY, card);
        name->setWordWrap(true);

        Row row;
        row.permission = item.permission;
        row.open_button = new Button(tr("Open"), Button::Role::FILLED, card);
        row.granted_label = new Label(tr("Granted"), Label::Role::CAPTION, card);

        const Permission permission = item.permission;
        connect(row.open_button, &Button::clicked, this, [permission]() { openSettings(permission); });

        QHBoxLayout* card_row = new QHBoxLayout();
        card_row->setContentsMargins(0, 0, 0, 0);
        card_row->addWidget(name, 1);
        card_row->addWidget(row.open_button);
        card_row->addWidget(row.granted_label);

        card->contentLayout()->addLayout(card_row);
        layout->addWidget(card);

        rows_.append(row);
    }

    layout->addStretch();
    scroll_area->setWidget(content);

    QVBoxLayout* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(0, 0, 0, 0);
    outer_layout->addWidget(scroll_area);
}

//--------------------------------------------------------------------------------------------------
PermissionsWidget::~PermissionsWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool PermissionsWidget::refresh()
{
    bool all_granted = true;

    for (const Row& row : std::as_const(rows_))
    {
        const bool granted = isGranted(row.permission);

        row.open_button->setVisible(!granted);
        row.granted_label->setVisible(granted);

        if (!granted)
            all_granted = false;
    }

    return all_granted;
}

//--------------------------------------------------------------------------------------------------
// static
bool PermissionsWidget::isGranted(Permission permission)
{
    // Without a context nothing can be checked, and the app must not be locked for good.
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return true;

    switch (permission)
    {
        case Permission::ACCESSIBILITY:
            return QJniObject::callStaticMethod<jboolean>(
                kInputServiceClass, "isEnabled", "(Landroid/content/Context;)Z", context.object());

        case Permission::OVERLAY:
            return QJniObject::callStaticMethod<jboolean>(
                kMenuClass, "canDraw", "(Landroid/content/Context;)Z", context.object());

        case Permission::STORAGE:
            return QJniObject::callStaticMethod<jboolean>(
                "android/os/Environment", "isExternalStorageManager", "()Z");

        case Permission::NOTIFICATIONS:
            return QJniObject::callStaticMethod<jboolean>(
                kActivityClass, "areNotificationsEnabled", "(Landroid/content/Context;)Z",
                context.object());
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void PermissionsWidget::openSettings(Permission permission)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([permission]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        switch (permission)
        {
            case Permission::ACCESSIBILITY:
                QJniObject::callStaticMethod<void>(kInputServiceClass, "openSettings",
                    "(Landroid/content/Context;)V", context.object());
                break;

            case Permission::OVERLAY:
                QJniObject::callStaticMethod<void>(kMenuClass, "openPermissionSettings",
                    "(Landroid/content/Context;)V", context.object());
                break;

            case Permission::STORAGE:
                openStorageSettings(context);
                break;

            case Permission::NOTIFICATIONS:
                QJniObject::callStaticMethod<void>(kActivityClass, "openNotificationSettings",
                    "(Landroid/content/Context;)V", context.object());
                break;
        }

        // A device without the settings screen throws; clear the pending exception so later JNI is
        // safe.
        QJniEnvironment().checkAndClearExceptions();
        return QVariant();
    });
}
