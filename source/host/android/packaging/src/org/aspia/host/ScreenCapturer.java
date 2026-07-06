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
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionConfig;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;

import java.nio.ByteBuffer;

// MediaProjection screen capture backing ScreenCapturerAndroid. The native side asks for a capture via
// requestCapture(), which launches the consent Activity; on consent the ScreenCaptureService (a
// foreground service of type mediaProjection, required to obtain the projection on modern Android)
// calls startProjection() and frames are forwarded to nativeOnFrame() on a dedicated handler thread. A
// single capture session is active at a time (the host serves one desktop), so the state is kept static
// and the token identifies the owning native capturer.
public final class ScreenCapturer
{
    private static final String TAG = "Aspia";

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

    // Returns the Intent that asks the user for screen capture consent. On Android 14+ the consent
    // dialog is configured to capture the whole display, so it defaults to (and is locked on) the
    // entire screen instead of a single app - the host needs the full screen.
    public static Intent createPermissionIntent(Activity activity)
    {
        MediaProjectionManager manager = (MediaProjectionManager)
            activity.getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
        {
            return manager.createScreenCaptureIntent(
                MediaProjectionConfig.createConfigForDefaultDisplay());
        }

        return manager.createScreenCaptureIntent();
    }

    // Real pixel geometry of the default display; usable before the projection exists so the native side
    // can report a screen resolution as soon as the capturer is created.
    private static DisplayMetrics realMetrics(Context context)
    {
        DisplayManager manager = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
        Display display = manager.getDisplay(Display.DEFAULT_DISPLAY);
        DisplayMetrics metrics = new DisplayMetrics();
        display.getRealMetrics(metrics);
        return metrics;
    }

    public static int displayWidth(Context context)
    {
        return realMetrics(context).widthPixels;
    }

    public static int displayHeight(Context context)
    {
        return realMetrics(context).heightPixels;
    }

    public static int displayDpi(Context context)
    {
        return realMetrics(context).densityDpi;
    }

    // Launches the transparent consent Activity for |token|. On consent it starts the foreground service
    // that owns the projection; on refusal notifyDenied() reports the failure to the native side.
    public static void requestCapture(Context context, long token)
    {
        Intent intent = new Intent(context, ScreenCapturePermissionActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ScreenCapturePermissionActivity.EXTRA_TOKEN, token);
        context.startActivity(intent);
    }

    // Reports that the user declined the screen capture consent.
    public static void notifyDenied(long token)
    {
        Log.i(TAG, "Screen capture consent denied");
        nativeOnStarted(token, false, 0, 0, 0);
    }

    // Starts the projection and begins delivering frames. Called from the foreground service so the
    // projection is obtained while a mediaProjection-type service is running. Reports the outcome and the
    // chosen virtual display geometry through nativeOnStarted().
    public static synchronized void startProjection(Context context, int resultCode, Intent data,
                                                    long token)
    {
        try
        {
            if (data == null)
            {
                Log.e(TAG, "Screen capture consent data is missing");
                nativeOnStarted(token, false, 0, 0, 0);
                return;
            }

            MediaProjectionManager manager = (MediaProjectionManager)
                context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            sProjection = manager.getMediaProjection(resultCode, data);
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

            DisplayMetrics metrics = realMetrics(context);
            final int width = metrics.widthPixels;
            final int height = metrics.heightPixels;
            final int dpi = metrics.densityDpi;

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

    // Stops the capture for |token|: stops the foreground service (which releases the projection in its
    // onDestroy) and releases directly in case the service was never started.
    public static void stopCapture(Context context, long token)
    {
        try
        {
            context.stopService(new Intent(context, ScreenCaptureService.class));
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to stop screen capture service", t);
        }

        stop(token);
    }

    // Releases all projection resources for |token|.
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
