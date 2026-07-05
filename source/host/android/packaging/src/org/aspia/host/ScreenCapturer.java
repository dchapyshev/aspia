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
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.WindowMetrics;

import java.nio.ByteBuffer;

// MediaProjection screen capture backing ScreenCapturerAndroid. The native side drives start()/stop()
// through JNI; produced images are forwarded to nativeOnFrame() on a dedicated handler thread. A single
// capture session is active at a time (the host serves one desktop), so the state is kept static and the
// token identifies the owning native capturer.
public final class ScreenCapturer
{
    private static final String TAG = "Aspia";

    // Screen capture consent, obtained from a MediaProjection permission Intent (Activity result). The
    // flow that requests it is wired in separately; start() reuses the stored result. getMediaProjection
    // also requires a running foreground service of type mediaProjection on modern Android.
    private static int sResultCode = Activity.RESULT_CANCELED;
    private static Intent sResultData = null;

    private static MediaProjection sProjection = null;
    private static VirtualDisplay sVirtualDisplay = null;
    private static ImageReader sReader = null;
    private static HandlerThread sHandlerThread = null;
    private static Handler sHandler = null;
    private static long sToken = 0;

    // Native callbacks implemented in screen_capturer_android.cc.
    private static native void nativeOnStarted(long token, boolean success, int width, int height,
                                               int dpi);
    private static native void nativeOnFrame(long token, ByteBuffer buffer, int width, int height,
                                             int pixelStride, int rowStride);

    private ScreenCapturer()
    {
        // Static-only.
    }

    // Returns the Intent that asks the user for screen capture consent; the resulting resultCode and
    // data Intent must be handed back through setPermissionResult() before start() is called.
    public static Intent createPermissionIntent(Activity activity)
    {
        MediaProjectionManager manager = (MediaProjectionManager)
            activity.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        return manager.createScreenCaptureIntent();
    }

    // Stores the consent result from the permission Intent for the next start().
    public static synchronized void setPermissionResult(int resultCode, Intent data)
    {
        sResultCode = resultCode;
        sResultData = data;
    }

    // Starts the projection and begins delivering frames. Reports the outcome and the chosen virtual
    // display geometry through nativeOnStarted() before returning.
    public static synchronized void start(Activity activity, long token)
    {
        try
        {
            if (sResultData == null)
            {
                Log.e(TAG, "Screen capture consent is not available");
                nativeOnStarted(token, false, 0, 0, 0);
                return;
            }

            MediaProjectionManager manager = (MediaProjectionManager)
                activity.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            sProjection = manager.getMediaProjection(sResultCode, sResultData);
            if (sProjection == null)
            {
                Log.e(TAG, "getMediaProjection returned null");
                nativeOnStarted(token, false, 0, 0, 0);
                return;
            }

            sHandlerThread = new HandlerThread("AspiaScreenCapture");
            sHandlerThread.start();
            sHandler = new Handler(sHandlerThread.getLooper());

            // A callback is mandatory; the projection is torn down from the native stop() path.
            sProjection.registerCallback(new MediaProjection.Callback()
            {
                @Override
                public void onStop()
                {
                    Log.i(TAG, "MediaProjection stopped");
                }
            }, sHandler);

            final int dpi = activity.getResources().getConfiguration().densityDpi;

            int width;
            int height;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            {
                WindowMetrics metrics = activity.getWindowManager().getCurrentWindowMetrics();
                Rect bounds = metrics.getBounds();
                width = bounds.width();
                height = bounds.height();
            }
            else
            {
                Point size = new Point();
                activity.getWindowManager().getDefaultDisplay().getRealSize(size);
                width = size.x;
                height = size.y;
            }

            sToken = token;

            sReader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2);
            sReader.setOnImageAvailableListener(reader ->
            {
                Image image = reader.acquireLatestImage();
                if (image == null)
                    return;

                try
                {
                    Image.Plane[] planes = image.getPlanes();
                    ByteBuffer buffer = planes[0].getBuffer();
                    nativeOnFrame(sToken, buffer, image.getWidth(), image.getHeight(),
                                  planes[0].getPixelStride(), planes[0].getRowStride());
                }
                finally
                {
                    image.close();
                }
            }, sHandler);

            sVirtualDisplay = sProjection.createVirtualDisplay("AspiaScreenCapture", width, height, dpi,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR, sReader.getSurface(), null, sHandler);

            Log.i(TAG, "Screen capture started: " + width + "x" + height + " @ " + dpi + " dpi");
            nativeOnStarted(token, true, width, height, dpi);
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to start screen capture", t);
            releaseInternal();
            nativeOnStarted(token, false, 0, 0, 0);
        }
    }

    // Stops the projection and releases all resources for |token|.
    public static synchronized void stop(long token)
    {
        if (token != sToken)
            return;

        releaseInternal();
    }

    private static void releaseInternal()
    {
        if (sVirtualDisplay != null)
        {
            sVirtualDisplay.release();
            sVirtualDisplay = null;
        }

        if (sReader != null)
        {
            sReader.close();
            sReader = null;
        }

        if (sProjection != null)
        {
            sProjection.stop();
            sProjection = null;
        }

        if (sHandlerThread != null)
        {
            sHandlerThread.quitSafely();
            sHandlerThread = null;
            sHandler = null;
        }

        sToken = 0;
    }
}
