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

package org.aspia.client;

import android.content.Intent;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;

import org.qtproject.qt.android.bindings.QtActivity;

// Disables Qt's in-app accessibility bridge before Qt starts. When any device-wide accessibility service
// is enabled (e.g. the Aspia host's input service, or a screen reader), Qt would otherwise activate its
// own accessibility bridge, which deadlocks with the OpenGL window surface and aborts on start
// (QtAndroidAccessibility vs QAndroidPlatformOpenGLWindow::eglSurface). Qt checks this environment
// variable in QtAccessibilityDelegate on the Java side, so it must be set before the base onCreate()
// runs the Qt setup.
public final class ClientActivity extends QtActivity
{
    // Registered from the C++ side (Application constructor).
    private static native void nativeUrlOpened(String url);

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

    // Delivers aspia:// links opened while the application is already running (launchMode is
    // singleTop, so the existing instance receives them here instead of a second one starting).
    @Override
    public void onNewIntent(Intent intent)
    {
        super.onNewIntent(intent);
        setIntent(intent);

        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction()))
            return;

        String url = intent.getDataString();
        if (url == null)
            return;

        try
        {
            nativeUrlOpened(url);
        }
        catch (UnsatisfiedLinkError e)
        {
            // The native side is not up yet. The URL stays in the activity intent (setIntent
            // above) and is picked up by the startup path.
        }
    }
}
