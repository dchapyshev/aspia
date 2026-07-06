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
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

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

        startActivityForResult(MediaProjection.createPermissionIntent(this), REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

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
