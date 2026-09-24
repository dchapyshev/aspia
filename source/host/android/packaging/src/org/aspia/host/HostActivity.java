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

package org.aspia.host;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.system.ErrnoException;
import android.system.Os;

import org.qtproject.qt.android.bindings.QtActivity;

// Disables Qt's in-app accessibility bridge before Qt starts. This app ships an accessibility service
// (InputService) for remote input, and whenever any accessibility service is enabled Qt would otherwise
// activate its own accessibility bridge, which deadlocks with the OpenGL window surface and aborts
// (QtAndroidAccessibility vs QAndroidPlatformOpenGLWindow::eglSurface). Qt checks this environment
// variable in QtAccessibilityDelegate on the Java side, so it must be set before the base onCreate()
// runs the Qt setup.
public final class HostActivity extends QtActivity
{
    // Covers both the permission of Android 13+ and the switch in the notification settings of the app.
    public static boolean areNotificationsEnabled(Context context)
    {
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        return manager == null || manager.areNotificationsEnabled();
    }

    // The request of the system is not used, because Android stops showing it once the user denied it
    // twice. The settings can always turn the notifications on.
    public static void openNotificationSettings(Context context)
    {
        Intent intent = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
        intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        try
        {
            Os.setenv("QT_ANDROID_DISABLE_ACCESSIBILITY", "1", true);
        }
        catch (ErrnoException e)
        {
            // Ignore: accessibility just stays enabled.
        }

        super.onCreate(savedInstanceState);
    }
}
