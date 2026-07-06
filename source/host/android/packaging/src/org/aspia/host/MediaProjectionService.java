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

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

// Foreground service of type mediaProjection. Obtaining a MediaProjection on modern Android requires
// such a service to be running, so the projection itself is started from here (in ScreenCapturer) right
// after the service enters the foreground.
public final class MediaProjectionService extends Service
{
    public static final String EXTRA_RESULT_CODE = "org.aspia.host.resultCode";
    public static final String EXTRA_RESULT_DATA = "org.aspia.host.resultData";

    private static final String CHANNEL_ID = "aspia_capture";
    private static final int NOTIFICATION_ID = 2;

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        createNotificationChannel();

        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("Aspia")
                .setContentText("Screen sharing is active")
                .setSmallIcon(android.R.drawable.ic_dialog_info)
                .setOngoing(true)
                .build();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
        {
            startForeground(NOTIFICATION_ID, notification,
                            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
        }
        else
        {
            startForeground(NOTIFICATION_ID, notification);
        }

        final int result_code = intent.getIntExtra(EXTRA_RESULT_CODE, Activity.RESULT_CANCELED);

        Intent result_data;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            result_data = intent.getParcelableExtra(EXTRA_RESULT_DATA, Intent.class);
        else
            result_data = intent.getParcelableExtra(EXTRA_RESULT_DATA);

        MediaProjection.startProjection(this, result_code, result_data);

        // Do not recreate the service if the system kills it; a new session starts it again.
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy()
    {
        MediaProjection.stop();
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }

    private void createNotificationChannel()
    {
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "Screen sharing", NotificationManager.IMPORTANCE_LOW);
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(channel);
    }
}
