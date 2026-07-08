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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.PowerManager;

// Reports display on/off to the native Server. Locking the screen does not change the Qt application
// state, so the host would otherwise stay online through the router while the screen is off. The
// SCREEN_ON/SCREEN_OFF broadcasts cannot be declared in the manifest (Android 8+), so the receiver is
// registered at runtime. onReceive is delivered on the main thread; the native side hops to its own.
public final class ScreenMonitor
{
    private static BroadcastReceiver receiver = null;

    public static native void nativeOnInteractiveChanged(boolean interactive);

    public static void start(Context context)
    {
        if (receiver != null)
            return;

        receiver = new BroadcastReceiver()
        {
            @Override
            public void onReceive(Context context, Intent intent)
            {
                String action = intent.getAction();
                if (Intent.ACTION_SCREEN_OFF.equals(action))
                    nativeOnInteractiveChanged(false);
                else if (Intent.ACTION_SCREEN_ON.equals(action))
                    nativeOnInteractiveChanged(true);
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);

        // Both actions are protected system broadcasts, so no exported flag is required; it is passed
        // on API 33+ anyway to satisfy the stricter registration checks.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            context.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        else
            context.registerReceiver(receiver, filter);

        // The broadcasts only report transitions, so push the current state: the screen may already be
        // off by the time the monitor starts (both run on the main thread, so this cannot interleave
        // with a broadcast).
        PowerManager power_manager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (power_manager != null)
            nativeOnInteractiveChanged(power_manager.isInteractive());
    }

    public static void stop(Context context)
    {
        if (receiver == null)
            return;

        context.unregisterReceiver(receiver);
        receiver = null;
    }
}
