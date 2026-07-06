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

#include "host/android/floating_menu_bridge.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

#include "base/logging.h"

namespace {

const char kOverlayClass[] = "org/aspia/host/FloatingMenu";

// The single live bridge, so the static JNI callback can reach it.
QMutex g_mutex;
FloatingMenuBridge* g_instance = nullptr;
bool g_registered = false;

//--------------------------------------------------------------------------------------------------
void JNICALL nativeOnClipboardText(JNIEnv* /* env */, jclass /* clazz */, jstring text)
{
    const QString value = QJniObject(text).toString();

    QMutexLocker locker(&g_mutex);
    if (g_instance)
        g_instance->onClipboardText(value);
}

//--------------------------------------------------------------------------------------------------
void ensureRegistered()
{
    if (g_registered)
        return;

    const JNINativeMethod methods[] =
    {
        {"nativeOnClipboardText", "(Ljava/lang/String;)V", reinterpret_cast<void*>(nativeOnClipboardText)},
    };

    QJniEnvironment env;
    if (env.registerNativeMethods(kOverlayClass, methods, std::size(methods)))
        g_registered = true;
    else
        LOG(ERROR) << "Unable to register FloatingMenu native methods";
}

} // namespace

//--------------------------------------------------------------------------------------------------
FloatingMenuBridge::FloatingMenuBridge(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";

    ensureRegistered();

    QMutexLocker locker(&g_mutex);
    g_instance = this;
}

//--------------------------------------------------------------------------------------------------
FloatingMenuBridge::~FloatingMenuBridge()
{
    LOG(INFO) << "Dtor";

    hideButton();

    QMutexLocker locker(&g_mutex);
    if (g_instance == this)
        g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FloatingMenuBridge::showButton()
{
    // The Java side hops to its own overlay thread; calling it directly (rather than through
    // runOnAndroidMainThread) keeps it working while the app is backgrounded, when the Android main thread
    // is blocked and would never run the posted work.
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "show",
            "(Landroid/content/Context;)V", context.object());
    }
}

//--------------------------------------------------------------------------------------------------
void FloatingMenuBridge::hideButton()
{
    QJniObject::callStaticMethod<void>(kOverlayClass, "hide", "()V");
}

//--------------------------------------------------------------------------------------------------
void FloatingMenuBridge::setIncomingText(const QString& text)
{
    QJniObject jni_text = QJniObject::fromString(text);
    QJniObject::callStaticMethod<void>(kOverlayClass, "setPending",
        "(Ljava/lang/String;)V", jni_text.object<jstring>());
}

//--------------------------------------------------------------------------------------------------
void FloatingMenuBridge::onClipboardText(const QString& text)
{
    // Called on the Android main thread; hop to this object's thread before emitting.
    QMetaObject::invokeMethod(this, [this, text]()
    {
        emit sig_clipboardText(text);
    }, Qt::QueuedConnection);
}
