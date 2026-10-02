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

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

// Keeps the process alive while the background mode is on, so the host stays reachable directly and
// through the router with the app off the screen, and tells the user about it. Android does not allow
// starting a foreground service from the background, so the native side starts it while the app is on
// the screen and stops it when the mode is turned off or the host stops.
public final class BackgroundModeService extends Service
{
    private static final String TAG = "Aspia";
    private static final String CHANNEL_ID = "aspia_background_mode";
    private static final int NOTIFICATION_ID = 4;

    private static final String EXTRA_TEXT = "org.aspia.host.text";

    // The text of the notification while the service runs, null otherwise. Set and cleared on the main
    // thread, read by start() on the thread of the host.
    private static volatile String sText = null;

    public static void start(Context context, String text)
    {
        // The host asks for the service on every return to the app. It is started again only when the
        // system stopped it or the text changed with the language.
        if (text != null && text.equals(sText))
            return;

        Intent intent = new Intent(context, BackgroundModeService.class);
        intent.putExtra(EXTRA_TEXT, text);

        try
        {
            context.startForegroundService(intent);
        }
        catch (Exception e)
        {
            // Not allowed from the background; the next return to the app starts it again.
            Log.w(TAG, "Unable to start the background mode service", e);
        }
    }

    public static void stop(Context context)
    {
        context.stopService(new Intent(context, BackgroundModeService.class));
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        String text = (intent != null) ? intent.getStringExtra(EXTRA_TEXT) : null;

        // The text of the notification names its channel in the settings of the app as well.
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, text != null ? text : "Aspia", NotificationManager.IMPORTANCE_LOW);
        getSystemService(NotificationManager.class).createNotificationChannel(channel);

        // A tap on the notification brings the app back, as the launcher does.
        Intent open_intent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent open_pending = PendingIntent.getActivity(
                this, 0, open_intent, PendingIntent.FLAG_IMMUTABLE);

        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("Aspia")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_dialog_info)
                .setContentIntent(open_pending)
                .setOngoing(true)
                .build();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else
            startForeground(NOTIFICATION_ID, notification);

        sText = text;

        // Do not recreate the service if the system kills it: without the app the host is not running
        // anyway. The next start of the app starts it again.
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy()
    {
        sText = null;
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }
}
