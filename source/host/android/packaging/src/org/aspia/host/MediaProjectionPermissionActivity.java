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
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;

// Transparent Activity that requests the MediaProjection consent. It exists because the consent dialog
// must be launched with startActivityForResult from an Activity; on grant it hands the result to the
// foreground service that owns the projection, on refusal it notifies the native side.
public final class MediaProjectionPermissionActivity extends Activity
{
    private static final int REQUEST_CODE = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Wake the screen and show over the keyguard, so the consent dialog renders (and can be
        // auto-confirmed) even when the screen was off or the device is locked.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1)
        {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }
        else
        {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
        }

        // Dismiss a non-secure keyguard so the consent dialog is reachable on a locked device. A secure
        // keyguard (PIN, pattern, password) cannot be dismissed without the user's credentials and is
        // left in place; remote access while such a device is locked is not possible.
        KeyguardManager keyguard = (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);
        if (keyguard != null)
            keyguard.requestDismissKeyguard(this, null);

        startActivityForResult(MediaProjection.createPermissionIntent(this), REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

        // The session may have ended while the user was answering. Nothing waits for the capture then,
        // and started now it would keep the screen captured until the next session.
        if (!MediaProjection.isRequested())
        {
            finish();
            return;
        }

        if (requestCode == REQUEST_CODE && resultCode == RESULT_OK && data != null)
        {
            Intent intent = new Intent(this, MediaProjectionService.class);
            intent.putExtra(MediaProjectionService.EXTRA_RESULT_CODE, resultCode);
            intent.putExtra(MediaProjectionService.EXTRA_RESULT_DATA, data);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                startForegroundService(intent);
            else
                startService(intent);
        }
        else
        {
            MediaProjection.notifyDenied();
        }

        finish();
    }
}
