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

#include "client/android/session_keeper_android.h"

#include <QCoreApplication>
#include <QJniObject>
#include <QVariant>

namespace {

const char kServiceClass[] = "org.aspia.client.SessionKeeperService";

//--------------------------------------------------------------------------------------------------
QJniObject createServiceIntent(const QJniObject& context)
{
    QJniObject intent("android/content/Intent");
    if (!intent.isValid())
        return QJniObject();

    QJniObject package = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject class_name = QJniObject::fromString(kServiceClass);

    intent.callObjectMethod("setClassName",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
        package.object<jstring>(), class_name.object<jstring>());
    return intent;
}

} // namespace

//--------------------------------------------------------------------------------------------------
SessionKeeperAndroid::SessionKeeperAndroid(QObject* parent)
    : SessionKeeper(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperAndroid::acquire()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        QJniObject intent = createServiceIntent(context);
        if (!intent.isValid())
            return QVariant();

        context.callObjectMethod("startForegroundService",
            "(Landroid/content/Intent;)Landroid/content/ComponentName;", intent.object());
        return QVariant();
    });
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperAndroid::release()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        QJniObject intent = createServiceIntent(context);
        if (!intent.isValid())
            return QVariant();

        context.callMethod<jboolean>("stopService", "(Landroid/content/Intent;)Z", intent.object());
        return QVariant();
    });
}
