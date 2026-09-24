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

// Keeps the process alive while the host waits for a connection in the background after the share,
// and tells the user about it. The share starts it while the app is still in the foreground, because
// Android does not allow starting a foreground service from the background. The native side stops it.
public final class ConnectionWaitService extends Service
{
    private static final String CHANNEL_ID = "aspia_connection_wait";
    private static final int NOTIFICATION_ID = 3;

    private static final String ACTION_CANCEL = "org.aspia.host.CANCEL_CONNECTION_WAIT";
    private static final String EXTRA_TEXT = "org.aspia.host.text";
    private static final String EXTRA_CANCEL_TEXT = "org.aspia.host.cancelText";

    public static native void nativeOnCancelled();

    public static void start(Context context, String text, String cancel_text)
    {
        Intent intent = new Intent(context, ConnectionWaitService.class);
        intent.putExtra(EXTRA_TEXT, text);
        intent.putExtra(EXTRA_CANCEL_TEXT, cancel_text);
        context.startForegroundService(intent);
    }

    public static void stop(Context context)
    {
        context.stopService(new Intent(context, ConnectionWaitService.class));
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        if (intent != null && ACTION_CANCEL.equals(intent.getAction()))
        {
            nativeOnCancelled();
            stopSelf();
            return START_NOT_STICKY;
        }

        String text = (intent != null) ? intent.getStringExtra(EXTRA_TEXT) : null;
        String cancel_text = (intent != null) ? intent.getStringExtra(EXTRA_CANCEL_TEXT) : null;

        // The text of the notification names its channel in the settings of the app as well.
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, text != null ? text : "Aspia", NotificationManager.IMPORTANCE_LOW);
        getSystemService(NotificationManager.class).createNotificationChannel(channel);

        // A tap on the notification brings the app back, as the launcher does.
        Intent open_intent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent open_pending = PendingIntent.getActivity(
                this, 0, open_intent, PendingIntent.FLAG_IMMUTABLE);

        Intent cancel_intent = new Intent(this, ConnectionWaitService.class);
        cancel_intent.setAction(ACTION_CANCEL);
        PendingIntent cancel_pending = PendingIntent.getService(
                this, 0, cancel_intent, PendingIntent.FLAG_IMMUTABLE);

        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("Aspia")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_dialog_info)
                .setContentIntent(open_pending)
                .addAction(new Notification.Action.Builder(null, cancel_text, cancel_pending).build())
                .setOngoing(true)
                .build();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else
            startForeground(NOTIFICATION_ID, notification);

        // Do not recreate the service if the system kills it; the next share starts it again.
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy()
    {
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }
}
